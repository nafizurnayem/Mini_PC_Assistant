import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

import psutil

from .protocol import timestamp


def run(args, timeout=3):
    result = subprocess.run(args, capture_output=True, text=True, timeout=timeout,
                            check=True, env={**os.environ, "LC_ALL": "C"})
    return result.stdout.strip()


def optional(args, timeout=3):
    try:
        return run(args, timeout)
    except (OSError, subprocess.SubprocessError):
        return ""


def number(value):
    try:
        result = float(value)
        return result if math.isfinite(result) else None
    except (TypeError, ValueError):
        return None


def available(reader, fallback, *args, **kwargs):
    """Optional OS facilities may be absent or restricted for an ordinary user."""
    try:
        return reader(*args, **kwargs)
    except (OSError, psutil.Error):
        return fallback


class Hardware:
    def __init__(self, interface=""):
        self.interface = interface
        self.previous = available(psutil.net_io_counters, {}, pernic=True)
        self.previous_time = time.monotonic()
        psutil.cpu_percent()

    def read(self):
        sensors = available(psutil.sensors_temperatures, {})
        cpu_temps = [s.current for key, entries in sensors.items()
                     if key in ("coretemp", "k10temp", "zenpower", "cpu_thermal")
                     for s in entries if s.current is not None]
        gpu = self.gpu(sensors)
        memory = psutil.virtual_memory()
        storage = []
        mounts = {p.mountpoint for p in available(psutil.disk_partitions, []) if p.fstype != "squashfs"}
        for mount in sorted(mounts | {"/"}):
            try:
                disk = psutil.disk_usage(mount)
                storage.append({"name": mount, "temperatureC": None,
                                "usedPercent": disk.percent, "freeBytes": disk.free,
                                "totalBytes": disk.total})
            except OSError:
                continue
        current = available(psutil.net_io_counters, {}, pernic=True)
        stats = available(psutil.net_if_stats, {})
        elapsed = max(time.monotonic() - self.previous_time, 0.001)
        names = [name for name in current if name != "lo" and (not stats or (stats.get(name) and stats[name].isup))]
        # Choose one interface to avoid counting traffic twice through VPN/bridge devices.
        def traffic(name):
            old = self.previous.get(name, current[name])
            return max(0, current[name].bytes_recv - old.bytes_recv) + max(0, current[name].bytes_sent - old.bytes_sent)
        name = self.interface or (max(names, key=traffic) if names else "")
        down = up = None
        if name in current and name in self.previous:
            down = max(0, current[name].bytes_recv - self.previous[name].bytes_recv) * 8 / elapsed / 1e6
            up = max(0, current[name].bytes_sent - self.previous[name].bytes_sent) * 8 / elapsed / 1e6
        self.previous, self.previous_time = current, time.monotonic()
        return {"timestamp": timestamp(),
                "cpu": {"temperatureC": max(cpu_temps) if cpu_temps else None, "loadPercent": psutil.cpu_percent()},
                "gpu": gpu,
                "memory": {"usedPercent": memory.percent, "usedBytes": memory.total - memory.available,
                           "totalBytes": memory.total},
                "storage": storage,
                "network": {"downloadMbps": round(down, 3) if down is not None else None,
                            "uploadMbps": round(up, 3) if up is not None else None, "interfaceName": name},
                "uptimeSeconds": max(0, int(time.time() - psutil.boot_time()))}

    @staticmethod
    def gpu(sensors):
        if shutil.which("nvidia-smi"):
            output = optional(["nvidia-smi", "--query-gpu=temperature.gpu,utilization.gpu", "--format=csv,noheader,nounits"])
            if output:
                values = output.splitlines()[0].split(",")
                if len(values) == 2:
                    return {"temperatureC": number(values[0]), "loadPercent": number(values[1])}
        temps = [s.current for key, entries in sensors.items() if key in ("amdgpu", "nouveau")
                 for s in entries if s.current is not None]
        loads = []
        for path in Path("/sys/class/drm").glob("card[0-9]*/device/gpu_busy_percent"):
            try:
                value = number(path.read_text())
                if value is not None:
                    loads.append(value)
            except OSError:
                pass
        return {"temperatureC": max(temps) if temps else None, "loadPercent": max(loads) if loads else None}


class Desktop:
    def volume(self):
        if shutil.which("pactl"):
            output = optional(["pactl", "get-sink-volume", "@DEFAULT_SINK@"])
            muted = optional(["pactl", "get-sink-mute", "@DEFAULT_SINK@"])
            match = re.search(r"(\d+)%", output)
            if match and muted:
                return {"level": min(100, int(match[1])), "muted": muted.endswith("yes"), "timestamp": timestamp()}
        if shutil.which("wpctl"):
            output = optional(["wpctl", "get-volume", "@DEFAULT_AUDIO_SINK@"])
            match = re.search(r"Volume: ([\d.]+)", output)
            if match:
                return {"level": min(100, round(float(match[1]) * 100)), "muted": "MUTED" in output, "timestamp": timestamp()}
        return None

    def set_volume(self, level):
        if shutil.which("pactl"):
            run(["pactl", "set-sink-volume", "@DEFAULT_SINK@", f"{level}%"])
        elif shutil.which("wpctl"):
            run(["wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", f"{level}%"])
        else:
            raise RuntimeError("Install pulseaudio-utils (pactl) or WirePlumber (wpctl) for volume control")

    def media(self):
        state = {"status": "stopped", "title": "", "artist": "", "album": "", "app": "",
                 "positionSeconds": None, "durationSeconds": None, "timestamp": timestamp()}
        if not shutil.which("playerctl"):
            return state
        players = optional(["playerctl", "--list-all"]).splitlines()
        selected = None
        status = "stopped"
        for player in players[:16]:
            candidate = optional(["playerctl", "--player", player, "status"]).lower()
            if selected is None or candidate == "playing":
                selected, status = player, candidate
            if candidate == "playing":
                break
        if selected is None:
            return state
        state.update(status=status if status in ("playing", "paused") else "stopped", app=selected)
        for key in ("title", "artist", "album"):
            state[key] = optional(["playerctl", "--player", selected, "metadata", "--format", "{{" + key + "}}"])
        duration = number(optional(["playerctl", "--player", selected, "metadata", "mpris:length"]))
        state["durationSeconds"] = duration / 1e6 if duration and duration > 0 else None
        state["positionSeconds"] = number(optional(["playerctl", "--player", selected, "position"]))
        return state

    def shutdown(self):
        # logind/polkit decides authorization. Never use sudo, force, or bypass inhibitors.
        run(["systemctl", "--no-ask-password", "poweroff"], timeout=15)


def speedtest_executable(executable="speedtest"):
    if executable == "speedtest":
        bundled = Path.home() / ".local/share/pico-assistant/bin/speedtest"
        if bundled.is_file() and os.access(bundled, os.X_OK):
            return str(bundled)
    return shutil.which(executable) or executable


def speed_test(executable):
    state = {"status": "failed", "downloadMbps": None, "uploadMbps": None,
             "pingMs": None, "server": None, "error": None}
    try:
        result = json.loads(run([speedtest_executable(executable), "--accept-license", "--accept-gdpr", "--format=json"], timeout=150))
        state.update(status="complete", downloadMbps=result["download"]["bandwidth"] * 8 / 1e6,
                     uploadMbps=result["upload"]["bandwidth"] * 8 / 1e6,
                     pingMs=result["ping"]["latency"], server=result["server"]["name"])
    except FileNotFoundError:
        state["error"] = "Ookla Speedtest is not installed. Run python3 linux/install-speedtest.py from the project folder, then retry."
    except subprocess.TimeoutExpired:
        state["error"] = "Speed test timed out after 150 seconds. Check the Internet connection or try another server later."
    except subprocess.CalledProcessError as error:
        detail = (error.stderr or error.stdout or str(error)).strip()
        state["error"] = detail[:500]
        if "unrecognized arguments" in detail or "no such option" in detail:
            state["error"] = "This executable is not the Ookla CLI. Run python3 linux/install-speedtest.py, then set the executable to speedtest."
    except (OSError, subprocess.SubprocessError, ValueError, KeyError, TypeError) as error:
        state["error"] = str(error)
    return {**state, "timestamp": timestamp()}


def diagnostics():
    try:
        interfaces = psutil.net_if_addrs()
        error = None
    except (OSError, psutil.Error) as exception:
        interfaces = {}
        error = str(exception)
    tools = {name: shutil.which(name) for name in ("pactl", "wpctl", "playerctl", "nvidia-smi", "speedtest", "systemctl")}
    tools["speedtest"] = shutil.which(speedtest_executable())
    return {"tools": tools,
            "interfaces": {name: [{"address": a.address, "broadcast": a.broadcast} for a in addresses]
                           for name, addresses in interfaces.items() if name != "lo"},
            "interfaceError": error}

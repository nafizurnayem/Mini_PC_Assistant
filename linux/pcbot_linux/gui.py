import json
from pathlib import Path
import queue
import subprocess
import sys
import threading
import time
import webbrowser
import tkinter as tk
from tkinter import messagebox, ttk

from .agent import publish_command
from .config import config_path, load, save, state_dir
from .hardware import diagnostics
from .protocol import command, parse_time
from .wifi_setup import INSTRUCTIONS, SETUP_URL, save_wifi


def rate(value):
    return f"{value:.3f}" if isinstance(value, (int, float)) else "unavailable"


def network_lines(status):
    snapshot = status.get("telemetry", {})
    network = snapshot.get("network", {})
    lines = [f"Live network ↓ {rate(network.get('downloadMbps'))} Mbps   ↑ {rate(network.get('uploadMbps'))} Mbps",
             f"Interface: {network.get('interfaceName') or 'unavailable'}   •   Traffic averaged over the sampling interval"]
    result = status.get("speedTest") or {}
    if result.get("status") == "complete":
        lines.append(f"Speed test ↓ {rate(result.get('downloadMbps'))} Mbps   ↑ {rate(result.get('uploadMbps'))} Mbps   •   Ping {rate(result.get('pingMs'))} ms")
    elif result.get("status") == "running":
        lines.append("Speed test running… Live traffic continues updating.")
    elif result:
        lines.append(f"Speed test failed: {result.get('error') or 'Unknown error'}")
    return lines


class ControlPanel:
    def __init__(self, root, path=None):
        self.root, self.path = root, Path(path or config_path())
        self.config = load(self.path)
        self.process = None
        self.events = queue.Queue()
        self.fields = {}
        self.busy = False
        root.title("Pico Assistant · Linux")
        root.geometry("820x670")
        root.minsize(700, 570)
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TFrame", background="#17212b")
        style.configure("TLabel", background="#17212b", foreground="#e4edf5", padding=4)
        style.configure("TCheckbutton", background="#17212b", foreground="#e4edf5")
        style.configure("Title.TLabel", font=("Sans", 23, "bold"))
        container = ttk.Frame(root, padding=20)
        container.pack(fill="both", expand=True)
        ttk.Label(container, text="Pico Assistant", style="Title.TLabel").pack(anchor="w")
        ttk.Label(container, text="Ubuntu desktop  /  Cloud MQTT  /  ESP32 & phone").pack(anchor="w", pady=(0, 12))
        tabs = ttk.Notebook(container)
        tabs.pack(fill="both", expand=True)
        dashboard, connection, monitoring, device, wifi = [ttk.Frame(tabs, padding=16) for _ in range(5)]
        for frame, label in zip((dashboard, connection, monitoring, device, wifi), ("Dashboard", "Connection", "Monitoring", "Device", "Bot Wi-Fi setup")):
            tabs.add(frame, text=label)

        ttk.Label(wifi, text="Set up or recover your bot's Wi-Fi", font=("Sans", 15, "bold")).pack(anchor="w")
        ttk.Label(wifi, text=INSTRUCTIONS, wraplength=710).pack(anchor="w", pady=12)
        self.wifi_ssid, self.wifi_password = tk.StringVar(), tk.StringVar()
        for label, variable, secret in (("Home Wi-Fi name (SSID)", self.wifi_ssid, False), ("New Wi-Fi password", self.wifi_password, True)):
            ttk.Label(wifi, text=label).pack(anchor="w")
            ttk.Entry(wifi, textvariable=variable, show="•" if secret else "", width=50).pack(fill="x", pady=(0, 8))
        ttk.Button(wifi, text="Save Wi-Fi to bot", command=self.save_bot_wifi).pack(anchor="w", pady=8)
        ttk.Button(wifi, text="Open setup page · 192.168.4.1", command=lambda: webbrowser.open(SETUP_URL)).pack(anchor="w", pady=4)
        ttk.Button(wifi, text="Enable setup on online bot…", command=lambda: self.reset("wifi")).pack(anchor="w", pady=4)
        self.wifi_result = tk.StringVar()
        ttk.Label(wifi, textvariable=self.wifi_result, wraplength=710).pack(anchor="w", pady=8)

        self.summary = tk.StringVar(value="Save your MQTT settings, then start the agent.")
        ttk.Label(dashboard, textvariable=self.summary, wraplength=720, font=("Sans", 11)).pack(anchor="w", pady=10)
        controls = ttk.Frame(dashboard)
        controls.pack(anchor="w", pady=12)
        for label, action in (("Start agent", self.start), ("Stop agent", self.stop), ("Run speed test", self.speed)):
            ttk.Button(controls, text=label, command=action).pack(side="left", padx=(0, 8))
        ttk.Button(controls, text="Bot Wi-Fi setup", command=lambda: tabs.select(wifi)).pack(side="left")
        ttk.Label(dashboard, text="The installed service keeps running when this window closes.\nAn agent started directly from the source folder stops when this window closes.", wraplength=700).pack(anchor="w")
        self.info = tk.StringVar(value="")
        ttk.Label(dashboard, textvariable=self.info, wraplength=710).pack(anchor="w", pady=16)

        fields = [("Device ID", "deviceId", self.config["deviceId"]),
                  ("MQTT host", "host", self.config["mqtt"]["host"]),
                  ("TLS port", "port", self.config["mqtt"]["port"]),
                  ("Username", "username", self.config["mqtt"]["username"]),
                  ("Password", "password", self.config["mqtt"]["password"]),
                  ("Custom CA file (optional)", "caFile", self.config["mqtt"]["caFile"])]
        for row, (label, key, value) in enumerate(fields):
            self.field(connection, row, label, key, value)
        ttk.Label(connection, text="Use the same device ID and broker as the phone and ESP32.\nUse a separate MQTT credential for Linux. TLS certificate verification is always enabled.", wraplength=690).grid(row=6, column=0, columnspan=2, sticky="w", pady=15)
        ttk.Button(connection, text="Save settings", command=self.save).grid(row=7, column=1, sticky="e")

        for row, (label, key) in enumerate((("Telemetry interval (seconds)", "telemetryIntervalSeconds"),
                                           ("Alert sustain (seconds)", "alertSustainSeconds"),
                                           ("Alert cooldown (seconds)", "alertCooldownSeconds"),
                                           ("Network interface (blank = automatic)", "networkInterface"),
                                           ("Ookla Speedtest executable", "speedTestExecutable"))):
            self.field(monitoring, row, label, key, self.config[key])
        for row, key in enumerate(self.config["thresholds"], start=5):
            self.field(monitoring, row, key, key, self.config["thresholds"][key])
        self.allow_shutdown = tk.BooleanVar(value=self.config["allowShutdown"])
        ttk.Checkbutton(monitoring, text="Allow graceful shutdown from phone / bot", variable=self.allow_shutdown).grid(row=11, column=0, columnspan=2, sticky="w", pady=8)
        ttk.Button(monitoring, text="Save settings", command=self.save).grid(row=12, column=1, sticky="e")

        ttk.Label(device, text="Wake-on-LAN is sent by the ESP32. Configure your wired Ethernet MAC\nand broadcast address in the bot's provisioning page.", wraplength=700).pack(anchor="w")
        text = tk.Text(device, height=14, background="#101820", foreground="#e4edf5", wrap="word", relief="flat")
        text.insert("1.0", json.dumps(diagnostics(), indent=2))
        text.configure(state="disabled")
        text.pack(fill="both", expand=True, pady=12)
        row = ttk.Frame(device)
        row.pack(anchor="w")
        ttk.Button(row, text="Reset bot Wi-Fi…", command=lambda: self.reset("wifi")).pack(side="left", padx=(0, 10))
        ttk.Button(row, text="Factory reset bot…", command=lambda: self.reset("factory")).pack(side="left")
        root.protocol("WM_DELETE_WINDOW", self.close)
        root.after(100, self.refresh)

    def field(self, frame, row, label, key, value):
        ttk.Label(frame, text=label).grid(row=row, column=0, sticky="w", pady=3)
        variable = tk.StringVar(value=str(value))
        self.fields[key] = variable
        ttk.Entry(frame, textvariable=variable, show="•" if key == "password" else "", width=40).grid(row=row, column=1, sticky="ew", padx=8)
        frame.columnconfigure(1, weight=1)

    def save(self):
        try:
            updated = load(self.path)
            updated["deviceId"] = self.fields["deviceId"].get().strip()
            for key in updated["mqtt"]:
                if key in self.fields:
                    updated["mqtt"][key] = self.fields[key].get() if key == "password" else self.fields[key].get().strip()
            updated["mqtt"]["port"] = int(updated["mqtt"]["port"])
            for key in ("telemetryIntervalSeconds", "alertSustainSeconds", "alertCooldownSeconds"):
                updated[key] = int(self.fields[key].get())
            for key in ("networkInterface", "speedTestExecutable"):
                updated[key] = self.fields[key].get().strip()
            for key in updated["thresholds"]:
                updated["thresholds"][key] = float(self.fields[key].get())
            updated["allowShutdown"] = self.allow_shutdown.get()
            save(updated, self.path)
            self.config = updated
            self.info.set("Settings saved. Stop and start the agent to apply them.")
            return True
        except (OSError, ValueError, TypeError) as error:
            messagebox.showerror("Settings", str(error))
            return False

    def background(self, task, success):
        if self.busy:
            return
        self.busy = True
        self.info.set("Working…")

        def worker():
            try:
                task()
                self.events.put((True, success))
            except Exception as error:
                self.events.put((False, str(error)))
        threading.Thread(target=worker, daemon=True).start()

    def installed(self):
        return (Path.home() / ".config/systemd/user/pico-assistant.service").is_file() and self.path == config_path()

    def start(self):
        if self.busy or not self.save():
            return
        if self.installed():
            self.background(lambda: subprocess.run(["systemctl", "--user", "restart", "pico-assistant.service"], check=True, capture_output=True, timeout=20), "Agent started")
        elif self.process is None or self.process.poll() is not None:
            state_dir().mkdir(parents=True, exist_ok=True, mode=0o700)
            log = state_dir() / "agent.log"
            with log.open("a") as stream:
                log.chmod(0o600)
                self.process = subprocess.Popen([sys.executable, "-m", "pcbot_linux", "--config", str(self.path), "agent"], stdout=stream, stderr=subprocess.STDOUT)
            self.info.set(f"Agent starting. Log: {log}")
        else:
            self.info.set("Agent is already running. Stop it first to apply changed settings.")

    def stop(self):
        if self.installed():
            self.background(lambda: subprocess.run(["systemctl", "--user", "stop", "pico-assistant.service"], check=True, capture_output=True, timeout=20), "Agent stopped")
        elif self.process is not None and self.process.poll() is None:
            process = self.process
            def terminate():
                process.terminate()
                process.wait(timeout=20)
            self.background(terminate, "Agent stopped")

    def speed(self):
        self.background(lambda: publish_command(load(self.path), "command/speedtest", command("run")), "Speed-test request sent; results appear here and in the phone app")

    def save_bot_wifi(self):
        if self.busy:
            return
        ssid, password = self.wifi_ssid.get(), self.wifi_password.get()
        self.wifi_result.set("Sending to the local setup hotspot…")
        self.background(lambda: save_wifi(ssid, password), "Wi-Fi saved. Bot restarting; reconnect this PC to your home network.")

    def reset(self, action):
        message = "Erase all bot settings and restart provisioning?" if action == "factory" else "Clear the bot's Wi-Fi settings and restart provisioning?"
        if messagebox.askyesno("Reset bot", message):
            self.background(lambda: publish_command(load(self.path), "command/reset", command(action)), "Reset request sent to the bot")

    def refresh(self):
        while not self.events.empty():
            ok, message = self.events.get_nowait()
            self.busy = False
            self.info.set(message)
            self.wifi_result.set(message)
            if not ok:
                messagebox.showerror("Pico Assistant", message)
        if self.process is not None and self.process.poll() not in (None, 0):
            self.info.set(f"Agent exited. Check {state_dir() / 'agent.log'}")
        try:
            status = json.loads((state_dir() / "status.json").read_text())
            fresh = time.time() - parse_time(status["timestamp"]) < 15
            snapshot = status.get("telemetry", {})
            cpu, memory = snapshot.get("cpu", {}), snapshot.get("memory", {})
            media, volume, bot = status.get("media", {}), status.get("volume") or {}, status.get("bot", {})
            connection = "Cloud connected" if fresh and status["connected"] else "Cloud disconnected / agent stopped"
            lines = [connection, f"Device: {status['deviceId']}   •   Bot: {bot.get('status', 'unknown')}",
                     f"CPU: {cpu.get('loadPercent', '—')}%   •   RAM: {memory.get('usedPercent', '—')}%",
                     f"CPU temperature: {cpu.get('temperatureC') if cpu.get('temperatureC') is not None else 'unavailable'} °C",
                     *network_lines(status),
                     f"Volume: {volume.get('level', 'unavailable')}   •   Muted: {volume.get('muted', '—')}",
                     f"Media: {media.get('status', 'stopped')}  {media.get('title', '')}",
                     f"Artist: {media.get('artist', '')}", f"Status: {status.get('message', '')}"]
            if not fresh:
                lines.append("Values above are from the last agent update.")
            self.summary.set("\n".join(lines))
        except (OSError, ValueError, KeyError):
            pass
        self.root.after(1000, self.refresh)

    def close(self):
        if self.process is not None and self.process.poll() is None:
            self.process.terminate()
        self.root.destroy()


def launch(path=None):
    root = tk.Tk()
    ControlPanel(root, path)
    root.mainloop()

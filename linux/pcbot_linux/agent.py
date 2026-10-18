import json
import logging
from pathlib import Path
import queue
import socket
import ssl
import threading
import time

import paho.mqtt.client as mqtt

from .alerts import Alerts
from .config import state_dir, validate, write_private
from .hardware import Desktop, Hardware, speed_test
from .protocol import CommandGuard, decode, timestamp, volume_level

LOG = logging.getLogger(__name__)


def wire_payload(suffix, value):
    """Leave room for MQTT headers/topics in the ESP32's 2048-byte buffer."""
    value = dict(value)
    if suffix == "telemetry/system":
        value["storage"] = [dict(disk, name=disk["name"][:160]) for disk in value["storage"]]
    def encode():
        return json.dumps(value, allow_nan=False, ensure_ascii=False, separators=(",", ":"))
    payload = encode()
    while len(payload.encode("utf-8")) > 1800:
        if suffix == "telemetry/system" and value["storage"]:
            value["storage"].pop()
        elif suffix == "state/media":
            field = max(("title", "artist", "album", "app"), key=lambda key: len(value.get(key, "")))
            if not value.get(field):
                break
            value[field] = value[field][:len(value[field]) // 2]
        else:
            break
        payload = encode()
    return payload


def client_for(config, suffix):
    validate(config)
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                         client_id=f"pcbot-linux-{config['deviceId']}-{socket.gethostname()}-{suffix}",
                         protocol=mqtt.MQTTv311, clean_session=True)
    settings = config["mqtt"]
    client.username_pw_set(settings["username"], settings["password"])
    context = ssl.create_default_context(cafile=settings["caFile"] or None)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    client.tls_set_context(context)
    client.reconnect_delay_set(min_delay=1, max_delay=60)
    client.max_queued_messages_set(100)
    return client


class Agent:
    def __init__(self, config, directory=None, client=None, hardware=None, desktop=None):
        validate(config)
        self.config = config
        self.directory = Path(directory or state_dir())
        self.directory.mkdir(parents=True, exist_ok=True, mode=0o700)
        self.root = f"pcbot/v1/{config['deviceId']}/"
        self.client = client or client_for(config, "agent")
        self.hardware = hardware or Hardware(config["networkInterface"])
        self.desktop = desktop or Desktop()
        self.alerts = Alerts(config)
        self.stop_event, self.connected = threading.Event(), threading.Event()
        self.inbox = queue.Queue(maxsize=128)
        self.speed_running = threading.Lock()
        self.cache_path = self.directory / f"commands-{config['deviceId']}.json"
        # Fail closed on a corrupt replay cache instead of silently re-executing commands.
        seen = json.loads(self.cache_path.read_text()) if self.cache_path.exists() else {}
        if not isinstance(seen, dict) or any(not isinstance(k, str) or type(v) not in (int, float) for k, v in seen.items()):
            raise ValueError("Invalid command cache")
        self.guard = CommandGuard(seen)
        self.status = {"connected": False, "deviceId": config["deviceId"], "message": "Connecting"}
        self.client.will_set(self.root + "state/pc", json.dumps({"status": "offline", "source": "linux-agent"}), qos=1, retain=True)
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_connect_fail = self.on_connect_fail
        self.client.on_subscribe = self.on_subscribe
        self.client.on_message = self.on_message

    def publish(self, suffix, value, retain=False, wait=False):
        if not self.connected.is_set():
            return False
        info = self.client.publish(self.root + suffix, wire_payload(suffix, value), qos=1, retain=retain)
        if info.rc != mqtt.MQTT_ERR_SUCCESS:
            return False
        if wait:
            info.wait_for_publish(timeout=3)
            return info.is_published()
        return True

    def pc_state(self, status, wait=False):
        return self.publish("state/pc", {"status": status, "source": "linux-agent", "timestamp": timestamp()}, True, wait)

    def ack(self, command_id, status, message, wait=False):
        self.status["message"] = message
        return self.publish("event/ack", {"commandId": command_id, "source": "linux-agent", "status": status,
                                          "message": message, "timestamp": timestamp()}, wait=wait)

    def on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            self.status["message"] = f"MQTT refused connection: {reason_code}"
            LOG.warning(self.status["message"])
            return
        self.connected.set()
        self.status.update(connected=True, message="Connected; subscribing")
        client.subscribe([(self.root + suffix, qos) for suffix, qos in
                          (("command/power", 1), ("command/speedtest", 1), ("command/volume", 0), ("state/bot", 1))])
        self.pc_state("online")
        if self.status.get("speedTest"):
            self.publish("state/speedtest", self.status["speedTest"], True)

    def on_subscribe(self, client, userdata, mid, reason_codes, properties):
        if any(code.is_failure for code in reason_codes):
            self.status["message"] = "Broker denied a subscription; check Linux credential topic permissions"
            LOG.error(self.status["message"])
        else:
            self.status["message"] = "Connected"

    def on_disconnect(self, client, userdata, flags, reason_code, properties):
        self.connected.clear()
        self.status.update(connected=False, message="Disconnected; reconnecting")

    def on_connect_fail(self, client, userdata):
        self.status.update(connected=False, message="Connection failed; check host, network and TLS certificate")
        LOG.warning(self.status["message"])

    def on_message(self, client, userdata, message):
        if len(message.payload) > 16384:
            return
        try:
            self.inbox.put_nowait((message.topic, bytes(message.payload), message.retain))
        except queue.Full:
            LOG.warning("Command queue full; dropping message")

    def handle(self, topic, payload, retained=False):
        command_id = None
        try:
            value = decode(payload)
            if topic == self.root + "state/bot":
                self.status["bot"] = {k: value.get(k) for k in ("status", "ip", "version")}
                return
            if topic == self.root + "command/volume":
                self.desktop.set_volume(volume_level(value, retained))
                volume = self.desktop.volume()
                if volume:
                    self.publish("state/volume", volume, True)
                return
            if topic not in (self.root + "command/power", self.root + "command/speedtest"):
                return
            command_id = value.get("commandId")
            # Waking is owned by the ESP32. Do not compete with its acknowledgement.
            if topic.endswith("command/power") and value.get("action") == "on":
                return
            if not self.guard.accept(value, retained):
                return
            # Persist before any effect so reconnects and agent restarts cannot replay it.
            write_private(self.cache_path, self.guard.seen)
            if topic.endswith("command/power") and value["action"] == "off":
                if not self.config["allowShutdown"]:
                    self.ack(command_id, "rejected", "Enable remote shutdown in the Linux control panel first")
                    return
                self.pc_state("shutting-down", wait=True)
                try:
                    self.desktop.shutdown()
                except Exception:
                    self.pc_state("online")
                    raise
                self.ack(command_id, "accepted", "Graceful Linux shutdown requested", wait=True)
            elif topic.endswith("command/speedtest") and value["action"] == "run":
                if not self.speed_running.acquire(blocking=False):
                    self.ack(command_id, "rejected", "A speed test is already running")
                    return
                self.ack(command_id, "accepted", "Speed test started")
                threading.Thread(target=self.run_speed_test, daemon=True).start()
            else:
                self.ack(command_id, "rejected", "Unsupported action for this topic")
        except Exception as error:
            LOG.warning("Command failed: %s", error)
            self.status["message"] = str(error)
            if isinstance(command_id, str) and len(command_id) <= 128:
                self.ack(command_id, "rejected", str(error))

    def run_speed_test(self):
        try:
            self.status["speedTest"] = {"status": "running", "timestamp": timestamp()}
            self.save_status()
            self.publish("state/speedtest", self.status["speedTest"], True)
            result = speed_test(self.config["speedTestExecutable"])
            self.status["speedTest"] = result
            self.status["message"] = "Speed test complete" if result["status"] == "complete" else "Speed test failed; see result"
            self.save_status()
            self.publish("state/speedtest", result, True)
            if result["status"] == "failed":
                LOG.warning("Speed test failed: %s", result.get("error"))
        finally:
            self.speed_running.release()

    def sample(self):
        snapshot = self.hardware.read()
        self.status["telemetry"] = snapshot
        if self.connected.is_set():
            self.publish("telemetry/system", snapshot)
            for alert in self.alerts.evaluate(snapshot):
                self.publish("event/alert", alert)

    def desktop_sample(self):
        media, volume = self.desktop.media(), self.desktop.volume()
        self.status.update(media=media, volume=volume)
        if self.connected.is_set():
            self.publish("state/media", media, True)
            if volume:
                self.publish("state/volume", volume, True)

    def save_status(self):
        write_private(self.directory / "status.json", {**self.status, "timestamp": timestamp()})

    def run(self):
        settings = self.config["mqtt"]
        self.client.connect_async(settings["host"], settings["port"], keepalive=30)
        self.client.loop_start()
        next_sample = next_desktop = 0
        try:
            while not self.stop_event.is_set():
                try:
                    topic, payload, retained = self.inbox.get(timeout=0.2)
                    self.handle(topic, payload, retained)
                except queue.Empty:
                    pass
                now = time.monotonic()
                if now >= next_sample:
                    try:
                        self.sample()
                    except Exception:
                        LOG.exception("Telemetry read failed")
                    next_sample = now + self.config["telemetryIntervalSeconds"]
                if now >= next_desktop:
                    try:
                        self.desktop_sample()
                    except Exception:
                        LOG.exception("Desktop read failed")
                    self.save_status()
                    next_desktop = now + 2
        finally:
            try:
                self.pc_state("offline", wait=True)
            finally:
                self.client.disconnect()
                self.client.loop_stop()
                self.status.update(connected=False, message="Agent stopped")
                self.save_status()


def publish_command(config, suffix, value):
    """Short-lived control-panel connection; never publishes PC presence."""
    client = client_for(config, "control")
    connected = threading.Event()
    error = []

    def on_connect(client, userdata, flags, reason, properties):
        if reason.is_failure:
            error.append(str(reason))
        connected.set()

    client.on_connect = on_connect
    settings = config["mqtt"]
    client.connect_async(settings["host"], settings["port"], 30)
    client.loop_start()
    try:
        if not connected.wait(12):
            raise RuntimeError("MQTT connection timed out; check credentials, host and TLS")
        if error:
            raise RuntimeError(error[0])
        info = client.publish(f"pcbot/v1/{config['deviceId']}/{suffix}", json.dumps(value), qos=1, retain=False)
        info.wait_for_publish(timeout=5)
        if not info.is_published():
            raise RuntimeError("Broker did not acknowledge the publish")
    finally:
        client.disconnect()
        client.loop_stop()

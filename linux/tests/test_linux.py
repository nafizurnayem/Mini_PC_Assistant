import copy
from datetime import datetime, timezone
import json
from pathlib import Path
import ssl
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch

from pcbot_linux.agent import Agent, client_for, wire_payload
from pcbot_linux.alerts import Alerts
from pcbot_linux.config import DEFAULTS, load, save, validate
from pcbot_linux.hardware import Desktop, Hardware, diagnostics, speed_test
from pcbot_linux.gui import network_lines
from pcbot_linux.protocol import CommandGuard, command, decode, timestamp, volume_level


def config():
    result = copy.deepcopy(DEFAULTS)
    result["mqtt"].update(host="broker.example.test", username="linux", password="test-only")
    return result


def at(seconds):
    return datetime.fromtimestamp(seconds, timezone.utc).isoformat()


def envelope(**changes):
    return {**command("off"), "issuedAt": at(100), "expiresAt": at(130), **changes}


class ProtocolTests(unittest.TestCase):
    def test_payload_fits_esp32_with_many_mounts_and_unicode_media(self):
        snapshot = {"storage": [{"name": "/" + "x" * 200, "usedPercent": 25} for _ in range(100)]}
        payload = wire_payload("telemetry/system", snapshot)
        self.assertLessEqual(len(payload.encode()), 1800)
        self.assertEqual(len(snapshot["storage"]), 100)
        media = {key: "🎵" * 1000 for key in ("title", "artist", "album", "app")}
        self.assertLessEqual(len(wire_payload("state/media", media).encode()), 1800)

    def test_duplicate_and_expiry(self):
        guard = CommandGuard()
        value = envelope()
        self.assertTrue(guard.accept(value, now=110))
        self.assertFalse(guard.accept(value, now=111))
        with self.assertRaises(ValueError):
            guard.accept(value, now=130)

    def test_retained_commands_never_execute(self):
        with self.assertRaises(ValueError):
            CommandGuard().accept(envelope(), retained=True, now=110)

    def test_malformed_command_times(self):
        for changes in ({"issuedAt": at(200)}, {"expiresAt": at(99)}, {"issuedAt": "2026-01-01T12:00:00"},
                        {"issuedAt": None}, {"source": "unknown"}, {"commandId": ""}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                CommandGuard().accept(envelope(**changes), now=110)

    def test_volume_is_fresh_bounded_and_nonretained(self):
        value = {"level": 53, "source": "bot", "timestamp": at(100)}
        self.assertEqual(volume_level(value, now=101), 53)
        for now, retain, level in ((103, False, 53), (97, False, 53), (101, True, 53),
                                    (101, False, -1), (101, False, 101), (101, False, float("nan")), (101, False, True)):
            with self.subTest(now=now, retain=retain, level=level), self.assertRaises(ValueError):
                volume_level({**value, "level": level}, retained=retain, now=now)

    def test_cache_saturation_does_not_evict_live_ids(self):
        guard = CommandGuard({str(i): 130 for i in range(4096)})
        with self.assertRaises(ValueError):
            guard.accept(envelope(), now=110)
        self.assertFalse(guard.accept(envelope(commandId="0"), now=110))

    def test_bad_json_and_large_messages(self):
        for payload in (b"[]", b"null", b"{", b" " * 16385):
            with self.assertRaises(ValueError):
                decode(payload)


class ConfigTests(unittest.TestCase):
    def test_private_roundtrip_and_merge_defaults(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "config.json"
            save(config(), path)
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)
            self.assertEqual(load(path), config())
            path.write_text('{"mqtt":{"host":"other"}}')
            self.assertEqual(load(path)["mqtt"]["port"], 8883)

    def test_invalid_config(self):
        for key, value in (("deviceId", "a/#"), ("telemetryIntervalSeconds", 0),
                           ("allowShutdown", "yes"), ("alertCooldownSeconds", -1)):
            with self.subTest(key=key), self.assertRaises(ValueError):
                validate({**config(), key: value})
        for key, value in (("host", "mqtt://example.test"), ("port", 70000), ("password", ""), ("caFile", "/no/such/ca.pem")):
            settings = config()
            settings["mqtt"][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                validate(settings)

    def test_client_requires_verified_tls_and_bounded_queue(self):
        with patch("pcbot_linux.agent.mqtt.Client") as factory:
            client_for(config(), "test")
            context = factory.return_value.tls_set_context.call_args.args[0]
            self.assertTrue(context.check_hostname)
            self.assertEqual(context.verify_mode, ssl.CERT_REQUIRED)
            self.assertEqual(context.minimum_version, ssl.TLSVersion.TLSv1_2)
            factory.return_value.max_queued_messages_set.assert_called_once_with(100)


class AgentTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.client, self.desktop, self.hardware = Mock(), Mock(), Mock()
        self.client.publish.return_value.rc = 0
        self.client.publish.return_value.is_published.return_value = True
        self.settings = config()
        self.agent = self.make_agent()

    def make_agent(self):
        agent = Agent(self.settings, self.tmp.name, self.client, self.hardware, self.desktop)
        agent.connected.set()
        return agent

    def send(self, suffix, value, retained=False):
        self.agent.handle(self.agent.root + suffix, json.dumps(value).encode(), retained)

    def publications(self, suffix):
        return [json.loads(call.args[1]) for call in self.client.publish.call_args_list
                if call.args[0].endswith(suffix)]

    def test_shutdown_disabled_by_default(self):
        self.send("command/power", command("off"))
        self.desktop.shutdown.assert_not_called()
        self.assertEqual(self.publications("event/ack")[-1]["status"], "rejected")

    def test_shutdown_once_even_after_restart(self):
        self.settings["allowShutdown"] = True
        value = command("off")
        self.send("command/power", value)
        self.send("command/power", value)
        self.agent = self.make_agent()
        self.send("command/power", value)
        self.desktop.shutdown.assert_called_once_with()
        self.assertEqual(self.publications("event/ack")[-1]["status"], "accepted")

    def test_retained_off_is_rejected(self):
        self.settings["allowShutdown"] = True
        self.send("command/power", command("off"), retained=True)
        self.desktop.shutdown.assert_not_called()

    def test_expired_off_is_rejected(self):
        self.settings["allowShutdown"] = True
        self.send("command/power", envelope())
        self.desktop.shutdown.assert_not_called()

    def test_shutdown_denial_restores_online(self):
        self.settings["allowShutdown"] = True
        self.desktop.shutdown.side_effect = RuntimeError("Policy denied")
        self.send("command/power", command("off"))
        self.assertEqual(self.publications("state/pc")[-1]["status"], "online")
        self.assertEqual(self.publications("event/ack")[-1]["status"], "rejected")

    def test_wake_remains_owned_by_bot(self):
        self.send("command/power", command("on"))
        self.desktop.shutdown.assert_not_called()
        self.assertEqual(self.publications("event/ack"), [])

    def test_volume_and_bad_messages_do_not_affect_power(self):
        self.desktop.volume.return_value = {"level": 42, "muted": False, "timestamp": timestamp()}
        self.send("command/volume", {"level": 42, "source": "bot", "timestamp": timestamp()})
        self.desktop.set_volume.assert_called_once_with(42)
        self.agent.handle(self.agent.root + "command/power", b"bad json")
        self.desktop.shutdown.assert_not_called()

    def test_reconnect_resubscribes_and_announces_presence(self):
        reason = Mock(is_failure=False)
        for _ in range(2):
            self.agent.on_connect(self.client, None, None, reason, None)
            self.agent.on_disconnect(self.client, None, None, reason, None)
        self.assertEqual(self.client.subscribe.call_count, 2)
        subscriptions = dict(self.client.subscribe.call_args.args[0])
        self.assertEqual(subscriptions[self.agent.root + "command/volume"], 0)
        self.assertEqual(len(self.publications("state/pc")), 2)
        self.assertFalse(self.agent.connected.is_set())
        self.assertTrue(self.client.will_set.call_args.kwargs["retain"])

    def test_speed_test_serialized_and_published(self):
        self.agent.speed_running.acquire()
        self.send("command/speedtest", command("run"))
        self.assertEqual(self.publications("event/ack")[-1]["status"], "rejected")
        with patch("pcbot_linux.agent.speed_test", return_value={"status": "complete"}):
            self.agent.run_speed_test()
        self.assertFalse(self.agent.speed_running.locked())
        self.assertEqual([p["status"] for p in self.publications("state/speedtest")], ["running", "complete"])
        status = json.loads((Path(self.tmp.name) / "status.json").read_text())
        self.assertEqual(status["speedTest"]["status"], "complete")

    def test_speed_failure_visible_locally_and_republished_after_reconnect(self):
        self.agent.speed_running.acquire()
        with patch("pcbot_linux.agent.speed_test", return_value={"status": "failed", "error": "Missing CLI"}):
            self.agent.run_speed_test()
        status = json.loads((Path(self.tmp.name) / "status.json").read_text())
        self.assertEqual(status["speedTest"]["error"], "Missing CLI")
        self.client.reset_mock()
        self.agent.on_connect(self.client, None, None, Mock(is_failure=False), None)
        self.assertEqual(self.publications("state/speedtest")[-1]["error"], "Missing CLI")

    def test_status_does_not_save_bot_ota_secret(self):
        self.send("state/bot", {"status": "online", "ip": "192.0.2.1", "otaToken": "secret"}, retained=True)
        self.agent.save_status()
        self.assertNotIn("secret", (Path(self.tmp.name) / "status.json").read_text())

    def test_cache_write_failure_prevents_shutdown(self):
        self.settings["allowShutdown"] = True
        with patch("pcbot_linux.agent.write_private", side_effect=OSError("disk full")):
            self.send("command/power", command("off"))
        self.desktop.shutdown.assert_not_called()


class HardwareTests(unittest.TestCase):
    def test_pactl_volume_and_setpoint(self):
        with patch("pcbot_linux.hardware.shutil.which", return_value="/usr/bin/pactl"), patch("pcbot_linux.hardware.run") as run:
            run.side_effect = ["Volume: front-left: 32768 / 50% / -18 dB", "Mute: yes", ""]
            self.assertEqual(Desktop().volume()["level"], 50)
            Desktop().set_volume(25)
            self.assertEqual(run.call_args.args[0], ["pactl", "set-sink-volume", "@DEFAULT_SINK@", "25%"])

    def test_wpctl_fallback(self):
        with patch("pcbot_linux.hardware.shutil.which", side_effect=lambda name: name if name == "wpctl" else None), patch("pcbot_linux.hardware.run", return_value="Volume: 0.72 [MUTED]"):
            volume = Desktop().volume()
            self.assertEqual(volume["level"], 72)
            self.assertTrue(volume["muted"])

    def test_missing_desktop_tools(self):
        with patch("pcbot_linux.hardware.shutil.which", return_value=None):
            self.assertIsNone(Desktop().volume())
            self.assertEqual(Desktop().media()["status"], "stopped")
            with self.assertRaises(RuntimeError):
                Desktop().set_volume(50)

    def test_shutdown_never_uses_force_or_sudo(self):
        with patch("pcbot_linux.hardware.run") as run:
            Desktop().shutdown()
            self.assertEqual(run.call_args.args[0], ["systemctl", "--no-ask-password", "poweroff"])

    def test_speed_result_converts_bytes_to_bits(self):
        result = {"download": {"bandwidth": 12500000}, "upload": {"bandwidth": 2500000},
                  "ping": {"latency": 4.5}, "server": {"name": "Test"}}
        with patch("pcbot_linux.hardware.run", return_value=json.dumps(result)):
            state = speed_test("speedtest")
        self.assertEqual(state["downloadMbps"], 100)
        self.assertEqual(state["uploadMbps"], 20)
        self.assertEqual(state["status"], "complete")

    def test_speed_failure_and_timeout(self):
        for error in (FileNotFoundError("speedtest"), subprocess.TimeoutExpired("speedtest", 150)):
            with patch("pcbot_linux.hardware.run", side_effect=error):
                self.assertEqual(speed_test("speedtest")["status"], "failed")

    def test_speedtest_missing_executable_explains_how_to_install(self):
        with patch("pcbot_linux.hardware.run", side_effect=FileNotFoundError()):
            self.assertIn("install-speedtest.py", speed_test("speedtest")["error"])

    def test_speedtest_preserves_stderr(self):
        error = subprocess.CalledProcessError(1, ["speedtest"], stderr="Cannot reach test server")
        with patch("pcbot_linux.hardware.run", side_effect=error):
            self.assertEqual(speed_test("speedtest")["error"], "Cannot reach test server")

    def test_local_ookla_binary_works_without_user_bin_in_service_path(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / ".local/share/pico-assistant/bin/speedtest"
            binary.parent.mkdir(parents=True)
            binary.touch(mode=0o755)
            with patch("pcbot_linux.hardware.Path.home", return_value=Path(tmp)), patch("pcbot_linux.hardware.run", return_value="{}") as run:
                speed_test("speedtest")
                self.assertEqual(run.call_args.args[0][0], str(binary))


class DashboardTests(unittest.TestCase):
    def test_live_rates_zero_and_unavailable_are_distinct(self):
        status = {"telemetry": {"network": {"downloadMbps": 12.345, "uploadMbps": 0.0, "interfaceName": "wlp8s0"}}}
        lines = "\n".join(network_lines(status))
        self.assertIn("12.345 Mbps", lines)
        self.assertIn("0.000 Mbps", lines)
        self.assertIn("wlp8s0", lines)
        self.assertIn("unavailable", "\n".join(network_lines({})))

    def test_dashboard_displays_speed_test_progress_results_and_errors(self):
        for result, expected in (({"status": "running"}, "running"),
                                 ({"status": "complete", "downloadMbps": 100, "uploadMbps": 30, "pingMs": 4}, "100.000 Mbps"),
                                 ({"status": "failed", "error": "Server unreachable"}, "Server unreachable")):
            self.assertIn(expected, "\n".join(network_lines({"speedTest": result})))


class HardwareContractTests(unittest.TestCase):

    def test_restricted_interfaces_return_diagnostics(self):
        with patch("pcbot_linux.hardware.psutil.net_if_addrs", side_effect=PermissionError("restricted")):
            result = diagnostics()
        self.assertEqual(result["interfaces"], {})
        self.assertIn("restricted", result["interfaceError"])

    def test_snapshot_matches_shared_contract_with_missing_sensors(self):
        with patch("pcbot_linux.hardware.psutil.sensors_temperatures", return_value={}), patch("pcbot_linux.hardware.psutil.net_if_stats", side_effect=PermissionError()), patch("pcbot_linux.hardware.Hardware.gpu", return_value={"temperatureC": None, "loadPercent": None}):
            snapshot = Hardware().read()
        contract = json.loads((Path(__file__).resolve().parents[2] / "contracts/mqtt-contract.json").read_text())
        self.assertTrue(set(contract["telemetry"]["required"]) <= snapshot.keys())
        self.assertIsNone(snapshot["cpu"]["temperatureC"])
        self.assertGreater(snapshot["memory"]["totalBytes"], 0)
        json.dumps(snapshot, allow_nan=False)


class AlertTests(unittest.TestCase):
    def test_sustain_cooldown_and_recovery(self):
        alerts = Alerts(config())
        snapshot = {"cpu": {"temperatureC": 95}, "gpu": {"temperatureC": None},
                    "memory": {"usedPercent": 50}, "storage": []}
        self.assertEqual(alerts.evaluate(snapshot, now=0), [])
        self.assertEqual(alerts.evaluate(snapshot, now=14), [])
        self.assertTrue(alerts.evaluate(snapshot, now=15)[0]["active"])
        self.assertEqual(alerts.evaluate(snapshot, now=16), [])
        snapshot["cpu"]["temperatureC"] = 88
        self.assertEqual(alerts.evaluate(snapshot, now=17), [])
        snapshot["cpu"]["temperatureC"] = 84
        self.assertFalse(alerts.evaluate(snapshot, now=18)[0]["active"])

    def test_missing_readings_break_sustain(self):
        alerts = Alerts(config())
        snapshot = {"cpu": {"temperatureC": 95}, "gpu": {"temperatureC": None},
                    "memory": {"usedPercent": 50}, "storage": []}
        alerts.evaluate(snapshot, now=0)
        snapshot["cpu"]["temperatureC"] = None
        alerts.evaluate(snapshot, now=10)
        snapshot["cpu"]["temperatureC"] = 95
        self.assertEqual(alerts.evaluate(snapshot, now=20), [])


if __name__ == "__main__":
    unittest.main()

import copy
import json
import math
import os
from pathlib import Path
import re
import tempfile


DEFAULTS = {
    "deviceId": "desktop-01",
    "mqtt": {"host": "", "port": 8883, "username": "", "password": "", "caFile": ""},
    "telemetryIntervalSeconds": 5,
    "alertSustainSeconds": 15,
    "alertCooldownSeconds": 300,
    "thresholds": {"cpuTempC": 90, "gpuTempC": 88, "driveTempC": 70,
                   "memoryUsedPercent": 95, "driveFreePercent": 10, "hysteresis": 5},
    "networkInterface": "",
    "speedTestExecutable": "speedtest",
    "allowShutdown": False,
}


def config_path():
    return Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config")) / "pico-assistant/config.json"


def state_dir():
    return Path(os.environ.get("XDG_STATE_HOME", Path.home() / ".local/state")) / "pico-assistant"


def load(path=None):
    result = copy.deepcopy(DEFAULTS)
    path = Path(path or config_path())
    if path.exists():
        data = json.loads(path.read_text())
        if not isinstance(data, dict):
            raise ValueError("Configuration must be a JSON object")
        for key, value in data.items():
            if key in ("mqtt", "thresholds"):
                if not isinstance(value, dict):
                    raise ValueError(f"{key} must be an object")
                result[key].update(value)
            else:
                result[key] = value
    return result


def validate(config):
    if not isinstance(config["deviceId"], str) or not re.fullmatch(r"[A-Za-z0-9_-]{2,32}", config["deviceId"]):
        raise ValueError("Device ID must be 2–32 letters, numbers, underscores or hyphens")
    mqtt = config["mqtt"]
    for key in ("host", "username", "password"):
        if not isinstance(mqtt[key], str) or not mqtt[key].strip():
            raise ValueError(f"Enter MQTT {key}")
    if "://" in mqtt["host"] or any(c in mqtt["host"] for c in "/ \t\n"):
        raise ValueError("MQTT host must be a hostname, without a URL scheme or path")
    if type(mqtt["port"]) is not int or not 1 <= mqtt["port"] <= 65535:
        raise ValueError("MQTT port must be 1–65535")
    if not isinstance(mqtt["caFile"], str) or (mqtt["caFile"] and not Path(mqtt["caFile"]).is_file()):
        raise ValueError("CA file does not exist")
    for key, low, high in (("telemetryIntervalSeconds", 2, 30),
                           ("alertSustainSeconds", 0, 3600), ("alertCooldownSeconds", 1, 86400)):
        if type(config[key]) is not int or not low <= config[key] <= high:
            raise ValueError(f"{key} must be an integer from {low} to {high}")
    for key, value in config["thresholds"].items():
        if type(value) not in (int, float) or not math.isfinite(value) or not 0 <= value <= 150:
            raise ValueError(f"Invalid threshold: {key}")
    if type(config["allowShutdown"]) is not bool:
        raise ValueError("allowShutdown must be true or false")
    for key in ("networkInterface", "speedTestExecutable"):
        if not isinstance(config[key], str):
            raise ValueError(f"{key} must be text")


def write_private(path, value):
    """Atomic owner-only file, including when replacing an older config."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd, name = tempfile.mkstemp(prefix=".pico-", dir=path.parent)
    try:
        with os.fdopen(fd, "w") as stream:
            json.dump(value, stream, indent=2, allow_nan=False)
            stream.write("\n")
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def save(config, path=None):
    validate(config)
    write_private(path or config_path(), config)

from datetime import datetime, timezone
import json
import math
import time
import uuid

SOURCES = {"mobile", "home-assistant", "google-home", "bot", "tray"}


def timestamp():
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def parse_time(value):
    if not isinstance(value, str):
        raise ValueError("Timestamp must be text")
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo is None:
        raise ValueError("Timestamp must include timezone")
    return parsed.timestamp()


def decode(payload):
    if len(payload) > 16384:
        raise ValueError("Message too large")
    value = json.loads(payload)
    if not isinstance(value, dict):
        raise ValueError("Message must be an object")
    return value


class CommandGuard:
    def __init__(self, seen=None):
        self.seen = seen if seen is not None else {}

    def accept(self, value, retained=False, now=None):
        now = time.time() if now is None else now
        if retained:
            raise ValueError("Retained commands are forbidden")
        command_id = value.get("commandId")
        if not isinstance(command_id, str) or not 1 <= len(command_id) <= 128:
            raise ValueError("Missing or invalid commandId")
        if value.get("source") not in SOURCES or not isinstance(value.get("action"), str):
            raise ValueError("Invalid source or action")
        issued = parse_time(value.get("issuedAt"))
        expires = parse_time(value.get("expiresAt"))
        if expires <= now or issued > now + 60 or expires <= issued:
            raise ValueError("Expired command or invalid command time")
        self.seen = {key: expiry for key, expiry in self.seen.items() if expiry > now}
        if command_id in self.seen:
            return False
        # Never evict unexpired IDs: that would allow replays during flooding.
        if len(self.seen) >= 4096:
            raise ValueError("Command cache full; retry after existing commands expire")
        self.seen[command_id] = expires
        return True


def volume_level(value, retained=False, now=None):
    now = time.time() if now is None else now
    age = now - parse_time(value.get("timestamp"))
    level = value.get("level")
    if retained or age > 2 or age < -2:
        raise ValueError("Stale or retained volume setpoint")
    if value.get("source") not in SOURCES:
        raise ValueError("Invalid volume source")
    if type(level) not in (int, float) or not math.isfinite(level) or not 0 <= level <= 100:
        raise ValueError("Volume must be 0–100")
    return round(level)


def command(action):
    from datetime import timedelta
    now = datetime.now(timezone.utc)
    return {"commandId": str(uuid.uuid4()), "source": "tray", "action": action,
            "issuedAt": now.isoformat(), "expiresAt": (now + timedelta(seconds=30)).isoformat()}

import time
import uuid

from .protocol import timestamp


class Alerts:
    def __init__(self, config):
        self.config = config
        self.since, self.last, self.active = {}, {}, set()

    def evaluate(self, snapshot, now=None):
        now = time.monotonic() if now is None else now
        limits = self.config["thresholds"]
        candidates = [("cpuTemp", snapshot["cpu"]["temperatureC"], limits["cpuTempC"], False),
                      ("gpuTemp", snapshot["gpu"]["temperatureC"], limits["gpuTempC"], False),
                      ("memory", snapshot["memory"]["usedPercent"], limits["memoryUsedPercent"], False)]
        for disk in snapshot["storage"]:
            candidates.extend([(f"driveFree:{disk['name']}", 100 - disk["usedPercent"], limits["driveFreePercent"], True),
                               (f"driveTemp:{disk['name']}", disk["temperatureC"], limits["driveTempC"], False)])
        events = []
        for key, value, threshold, low in candidates:
            if value is None:
                self.since.pop(key, None)
                continue
            bad = value <= threshold if low else value >= threshold
            recovered = value >= threshold + limits["hysteresis"] if low else value <= threshold - limits["hysteresis"]
            active = None
            if bad:
                self.since.setdefault(key, now)
                if (now - self.since[key] >= self.config["alertSustainSeconds"] and
                        now - self.last.get(key, float("-inf")) >= self.config["alertCooldownSeconds"]):
                    self.active.add(key)
                    self.last[key] = now
                    active = True
            else:
                self.since.pop(key, None)
                if recovered and key in self.active:
                    self.active.remove(key)
                    active = False
            if active is not None:
                events.append({"alertId": str(uuid.uuid4()), "severity": "critical" if active and abs(value - threshold) >= 10 else "warning",
                               "metric": key, "value": value, "threshold": threshold,
                               "message": f"{key}: {value:.1f}" if active else f"{key} recovered",
                               "timestamp": timestamp(), "active": active})
        return events

using PcBot.Core;

namespace PcBot.Service;

public sealed class AlertEvaluator
{
    private readonly Dictionary<string, DateTimeOffset> _violations = new();
    private readonly Dictionary<string, DateTimeOffset> _lastAlerts = new();
    private readonly HashSet<string> _active = new();

    public IReadOnlyList<AlertEvent> Evaluate(TelemetrySnapshot telemetry, AgentOptions options)
    {
        var candidates = new List<(string Key, double? Value, double Threshold, bool LowIsBad, string Label)>
        {
            ("cpuTemp", telemetry.Cpu.TemperatureC, options.Thresholds.CpuTempC, false, "CPU temperature"),
            ("gpuTemp", telemetry.Gpu.TemperatureC, options.Thresholds.GpuTempC, false, "GPU temperature"),
            ("memory", telemetry.Memory.UsedPercent, options.Thresholds.MemoryUsedPercent, false, "RAM usage"),
        };
        candidates.AddRange(telemetry.Storage.SelectMany(s => new[]
        {
            ($"driveTemp:{s.Name}", s.TemperatureC, options.Thresholds.DriveTempC, false, $"Drive {s.Name} temperature"),
            ($"driveFree:{s.Name}", 100 - s.UsedPercent, options.Thresholds.DriveFreePercent, true, $"Drive {s.Name} free space"),
        }));

        var now = telemetry.Timestamp;
        var alerts = new List<AlertEvent>();
        foreach (var item in candidates.Where(x => x.Value.HasValue))
        {
            var bad = item.LowIsBad ? item.Value <= item.Threshold : item.Value >= item.Threshold;
            var recovered = item.LowIsBad
                ? item.Value >= item.Threshold + options.Thresholds.Hysteresis
                : item.Value <= item.Threshold - options.Thresholds.Hysteresis;
            if (bad) {
                _violations.TryAdd(item.Key, now);
                var sustained = now - _violations[item.Key] >= TimeSpan.FromSeconds(options.AlertSustainSeconds);
                var cooledDown = !_lastAlerts.TryGetValue(item.Key, out var last) ||
                                 now - last >= TimeSpan.FromSeconds(options.AlertCooldownSeconds);
                if (sustained && cooledDown) {
                    _lastAlerts[item.Key] = now;
                    _active.Add(item.Key);
                    alerts.Add(Create(item, now, true));
                }
            } else if (recovered) {
                _violations.Remove(item.Key);
                if (_active.Remove(item.Key)) alerts.Add(Create(item, now, false));
            }
        }
        return alerts;
    }

    private static AlertEvent Create(
        (string Key, double? Value, double Threshold, bool LowIsBad, string Label) item,
        DateTimeOffset now, bool active)
    {
        var value = item.Value!.Value;
        var severity = active && Math.Abs(value - item.Threshold) >= 10 ? "critical" : "warning";
        var message = active ? $"{item.Label}: {value:F1}" : $"{item.Label} recovered";
        return new AlertEvent(Guid.NewGuid().ToString(), severity, item.Key, value,
            item.Threshold, message, now, active);
    }
}


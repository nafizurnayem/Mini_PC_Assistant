using System.Text.Json.Serialization;

namespace PcBot.Core;

public sealed record CommandEnvelope(
    string CommandId,
    string Source,
    string Action,
    DateTimeOffset IssuedAt,
    DateTimeOffset ExpiresAt)
{
    public bool IsExpired(DateTimeOffset now) => ExpiresAt <= now || IssuedAt > now.AddMinutes(1);
}

public sealed record Metric(double? TemperatureC = null, double? LoadPercent = null);
public sealed record MemoryMetric(double UsedPercent, ulong UsedBytes, ulong TotalBytes);
public sealed record StorageMetric(string Name, double? TemperatureC, double UsedPercent, long FreeBytes, long TotalBytes);
public sealed record NetworkMetric(double DownloadMbps, double UploadMbps, string InterfaceName);

public sealed record TelemetrySnapshot(
    DateTimeOffset Timestamp,
    Metric Cpu,
    Metric Gpu,
    MemoryMetric Memory,
    IReadOnlyList<StorageMetric> Storage,
    NetworkMetric Network,
    long UptimeSeconds);

public sealed record AlertEvent(
    string AlertId,
    string Severity,
    string Metric,
    double Value,
    double Threshold,
    string Message,
    DateTimeOffset Timestamp,
    bool Active);

public sealed record Acknowledgement(
    string CommandId,
    string Source,
    string Status,
    string Message,
    DateTimeOffset Timestamp);

/// <summary>
/// Whatever the PC is currently playing, read from the Windows media session.
/// Status is "playing", "paused" or "stopped".
/// </summary>
public sealed record MediaState(
    string Status,
    string Title,
    string Artist,
    string Album,
    string App,
    double? PositionSeconds,
    double? DurationSeconds,
    DateTimeOffset Timestamp);

public sealed record VolumeState(
    int Level,
    bool Muted,
    DateTimeOffset Timestamp);

/// <summary>
/// Volume setpoint from the bot's knob. Deliberately not a CommandEnvelope: it
/// is a continuously updated absolute value, so a late one is worthless rather
/// than worth retrying.
/// </summary>
public sealed record VolumeSetpoint(
    int Level,
    string Source,
    DateTimeOffset Timestamp);

public sealed record SpeedTestState(
    string Status,
    double? DownloadMbps,
    double? UploadMbps,
    double? PingMs,
    string? Server,
    string? Error,
    DateTimeOffset Timestamp);

public sealed class AgentOptions
{
    public string DeviceId { get; set; } = "desktop-01";
    public MqttOptions Mqtt { get; set; } = new();
    public ThresholdOptions Thresholds { get; set; } = new();
    public int TelemetryIntervalSeconds { get; set; } = 5;
    public int AlertSustainSeconds { get; set; } = 15;
    public int AlertCooldownSeconds { get; set; } = 300;
    public string SpeedTestExecutable { get; set; } = "speedtest.exe";
}

public sealed class MqttOptions
{
    public string Host { get; set; } = "";
    public int Port { get; set; } = 8883;
    public string Username { get; set; } = "";
    public string Password { get; set; } = "";
}

public sealed class ThresholdOptions
{
    public double CpuTempC { get; set; } = 90;
    public double GpuTempC { get; set; } = 88;
    public double DriveTempC { get; set; } = 70;
    public double MemoryUsedPercent { get; set; } = 95;
    public double DriveFreePercent { get; set; } = 10;
    public double Hysteresis { get; set; } = 5;
}

public sealed class MqttTopics
{
    private readonly string _root;
    public MqttTopics(string deviceId) => _root = $"pcbot/v1/{deviceId}";
    public string PowerCommand => $"{_root}/command/power";
    public string SpeedTestCommand => $"{_root}/command/speedtest";
    public string ResetCommand => $"{_root}/command/reset";
    public string VolumeCommand => $"{_root}/command/volume";
    public string PcState => $"{_root}/state/pc";
    public string SpeedTestState => $"{_root}/state/speedtest";
    public string MediaState => $"{_root}/state/media";
    public string VolumeState => $"{_root}/state/volume";
    public string Telemetry => $"{_root}/telemetry/system";
    public string Alert => $"{_root}/event/alert";
    public string Ack => $"{_root}/event/ack";
}


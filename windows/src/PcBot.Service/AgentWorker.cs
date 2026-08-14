using System.Collections.Concurrent;
using System.Diagnostics;
using System.Text.Json;
using MQTTnet;
using MQTTnet.Client;
using MQTTnet.Protocol;
using PcBot.Core;

namespace PcBot.Service;

public sealed class AgentWorker(
    ILogger<AgentWorker> logger,
    HardwareMonitor hardware,
    AlertEvaluator alertEvaluator,
    SpeedTestRunner speedTest) : BackgroundService
{
    private readonly IMqttClient _client = new MqttFactory().CreateMqttClient();
    private readonly ConcurrentDictionary<string, DateTimeOffset> _seenCommands = new();
    private readonly SemaphoreSlim _commandLock = new(1, 1);
    private readonly JsonSerializerOptions _json = new(JsonSerializerDefaults.Web);
    private CancellationTokenSource? _speedTestCancellation;
    private AgentOptions _options = new();
    private MqttTopics _topics = new("desktop-01");

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _client.ApplicationMessageReceivedAsync += HandleMessageAsync;
        while (!stoppingToken.IsCancellationRequested)
        {
            try {
                _options = OptionsStore.Load();
                _topics = new MqttTopics(_options.DeviceId);
                ValidateOptions(_options);
                if (!_client.IsConnected) await ConnectAsync(stoppingToken);

                var snapshot = hardware.Read();
                await PublishAsync(_topics.Telemetry, snapshot, false, stoppingToken);
                foreach (var alert in alertEvaluator.Evaluate(snapshot, _options))
                    await PublishAsync(_topics.Alert, alert, false, stoppingToken);
                CleanupCommandCache(snapshot.Timestamp);
                await Task.Delay(TimeSpan.FromSeconds(Math.Max(2, _options.TelemetryIntervalSeconds)), stoppingToken);
            } catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested) {
                break;
            } catch (Exception ex) {
                logger.LogError(ex, "Agent loop failed; retrying");
                await Task.Delay(TimeSpan.FromSeconds(5), stoppingToken);
            }
        }

        if (_client.IsConnected) {
            await PublishPcStateAsync("offline", CancellationToken.None);
            await _client.DisconnectAsync();
        }
    }

    private async Task ConnectAsync(CancellationToken cancellationToken)
    {
        var will = JsonSerializer.Serialize(new { status = "offline", source = "windows-agent" }, _json);
        var builder = new MqttClientOptionsBuilder()
            .WithClientId($"pcbot-windows-{_options.DeviceId}-{Environment.MachineName}")
            .WithTcpServer(_options.Mqtt.Host, _options.Mqtt.Port)
            .WithCredentials(_options.Mqtt.Username, _options.Mqtt.Password)
            .WithCleanSession()
            .WithWillTopic(_topics.PcState)
            .WithWillPayload(will)
            .WithWillQualityOfServiceLevel(MqttQualityOfServiceLevel.AtLeastOnce)
            .WithWillRetain()
            .WithTlsOptions(tls => tls.UseTls());
        await _client.ConnectAsync(builder.Build(), cancellationToken);

        var subscriptions = new MqttClientSubscribeOptionsBuilder()
            .WithTopicFilter(f => f.WithTopic(_topics.PowerCommand).WithAtLeastOnceQoS())
            .WithTopicFilter(f => f.WithTopic(_topics.SpeedTestCommand).WithAtLeastOnceQoS())
            .WithTopicFilter(f => f.WithTopic(_topics.VolumeCommand).WithAtLeastOnceQoS())
            .Build();
        await _client.SubscribeAsync(subscriptions, cancellationToken);
        await PublishPcStateAsync("online", cancellationToken);
        logger.LogInformation("Connected to MQTT as {DeviceId}", _options.DeviceId);
    }

    private async Task HandleMessageAsync(MqttApplicationMessageReceivedEventArgs args)
    {
        await _commandLock.WaitAsync();
        try {
            var payload = args.ApplicationMessage.ConvertPayloadToString();
            var msgTopic = args.ApplicationMessage.Topic;

            // VolumeSetpoint is NOT a CommandEnvelope — it is a continuously updated
            // absolute value. Handle it first, before the CommandEnvelope path.
            if (msgTopic == _topics.VolumeCommand)
            {
                var setpoint = JsonSerializer.Deserialize<VolumeSetpoint>(payload, _json);
                if (setpoint is null) return;
                // Discard stale setpoints so a knob backlog doesn’t replay.
                if (DateTimeOffset.UtcNow - setpoint.Timestamp > TimeSpan.FromSeconds(2)) return;
                VolumeController.SetLevel(setpoint.Level);
                logger.LogInformation("Volume set to {Level}% by {Source}", setpoint.Level, setpoint.Source);
                return;
            }

            var command = JsonSerializer.Deserialize<CommandEnvelope>(payload, _json);
            if (command is null || string.IsNullOrWhiteSpace(command.CommandId)) return;
            if (command.IsExpired(DateTimeOffset.UtcNow)) {
                await AcknowledgeAsync(command.CommandId, "rejected", "Command expired");
                return;
            }
            if (!_seenCommands.TryAdd(command.CommandId, DateTimeOffset.UtcNow)) return;

            if (args.ApplicationMessage.Topic == _topics.PowerCommand && command.Action == "off") {
                await AcknowledgeAsync(command.CommandId, "accepted", "Graceful Windows shutdown scheduled");
                await PublishPcStateAsync("shutting-down", CancellationToken.None);
                await Task.Delay(750);
                StartWindowsShutdown();
            } else if (args.ApplicationMessage.Topic == _topics.SpeedTestCommand && command.Action == "run") {
                await AcknowledgeAsync(command.CommandId, "accepted", "Speed test started");
                _speedTestCancellation?.Cancel();
                _speedTestCancellation = new CancellationTokenSource(TimeSpan.FromMinutes(3));
                _ = RunSpeedTestAsync(_speedTestCancellation.Token);
            }
        } catch (Exception ex) {
            logger.LogError(ex, "Failed to process MQTT command");
        } finally {
            _commandLock.Release();
        }
    }

    private async Task RunSpeedTestAsync(CancellationToken cancellationToken)
    {
        await PublishAsync(_topics.SpeedTestState,
            new SpeedTestState("running", null, null, null, null, null, DateTimeOffset.UtcNow), true,
            CancellationToken.None);
        var result = await speedTest.RunAsync(_options.SpeedTestExecutable, cancellationToken);
        await PublishAsync(_topics.SpeedTestState, result, true, CancellationToken.None);
    }

    private async Task AcknowledgeAsync(string commandId, string status, string message) =>
        await PublishAsync(_topics.Ack,
            new Acknowledgement(commandId, "windows-agent", status, message, DateTimeOffset.UtcNow),
            false, CancellationToken.None);

    private Task PublishPcStateAsync(string status, CancellationToken cancellationToken) =>
        PublishAsync(_topics.PcState,
            new { status, source = "windows-agent", timestamp = DateTimeOffset.UtcNow }, true, cancellationToken);

    private async Task PublishAsync<T>(string topic, T value, bool retain, CancellationToken cancellationToken)
    {
        if (!_client.IsConnected) return;
        var message = new MqttApplicationMessageBuilder()
            .WithTopic(topic)
            .WithPayload(JsonSerializer.Serialize(value, _json))
            .WithQualityOfServiceLevel(MqttQualityOfServiceLevel.AtLeastOnce)
            .WithRetainFlag(retain)
            .Build();
        await _client.PublishAsync(message, cancellationToken);
    }

    private static void StartWindowsShutdown()
    {
        Process.Start(new ProcessStartInfo("shutdown.exe", "/s /t 5 /d p:0:0 /c \"PC Assistant requested shutdown\"")
        {
            UseShellExecute = false,
            CreateNoWindow = true,
        });
    }

    private static void ValidateOptions(AgentOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.DeviceId) || string.IsNullOrWhiteSpace(options.Mqtt.Host) ||
            string.IsNullOrWhiteSpace(options.Mqtt.Username) || string.IsNullOrWhiteSpace(options.Mqtt.Password))
            throw new InvalidOperationException($"Complete MQTT setup in {OptionsStore.ConfigPath}");
    }

    private void CleanupCommandCache(DateTimeOffset now)
    {
        foreach (var pair in _seenCommands.Where(x => now - x.Value > TimeSpan.FromHours(1)))
            _seenCommands.TryRemove(pair.Key, out _);
    }
}


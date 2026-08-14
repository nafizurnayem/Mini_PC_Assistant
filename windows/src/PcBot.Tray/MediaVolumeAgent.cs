using System.Text;
using System.Text.Json;
using MQTTnet;
using MQTTnet.Client;
using MQTTnet.Protocol;
using PcBot.Core;
using Windows.Media.Control;

namespace PcBot.Tray;

/// <summary>
/// Publishes what the PC is playing and applies volume setpoints from the bot's
/// knob. This lives in the tray app rather than the Windows service because the
/// media session and the audio endpoint both belong to the interactive user
/// session; a session 0 service cannot see either.
/// </summary>
public sealed class MediaVolumeAgent : IAsyncDisposable
{
    private static readonly JsonSerializerOptions Json = new(JsonSerializerDefaults.Web);
    private static readonly TimeSpan PollInterval = TimeSpan.FromSeconds(1);
    private static readonly TimeSpan ReconnectDelay = TimeSpan.FromSeconds(10);

    private readonly CancellationTokenSource _cancellation = new();
    private readonly SemaphoreSlim _publishLock = new(1, 1);
    private IMqttClient? _client;
    private MqttTopics? _topics;
    private GlobalSystemMediaTransportControlsSessionManager? _sessions;
    private SystemVolume? _volume;
    private string _lastMediaPayload = "";
    private int _lastPublishedLevel = -1;
    private bool _lastPublishedMute;
    private Task? _worker;

    public string Status { get; private set; } = "Not started";
    public bool Connected => _client is { IsConnected: true };
    /// <summary>Latest media state, or null before the first read.</summary>
    public MediaState? Media { get; private set; }
    public int Volume { get; private set; }
    public bool Muted { get; private set; }

    /// <summary>Raised on the worker thread whenever media or volume changes.</summary>
    public event Action? Updated;

    /// <summary>Applies a level from the desktop UI and echoes it to the broker.</summary>
    public async Task SetVolumeAsync(int level)
    {
        if (_volume is null) return;
        _volume.SetLevel(level);
        await PublishVolumeAsync(force: true, _cancellation.Token);
    }

    public void Start()
    {
        _worker ??= Task.Run(() => RunAsync(_cancellation.Token));
    }

    private async Task RunAsync(CancellationToken token)
    {
        while (!token.IsCancellationRequested)
        {
            try
            {
                await ConnectAsync(token);
                Status = "Connected";
                while (!token.IsCancellationRequested && _client is { IsConnected: true })
                {
                    await PublishMediaAsync(token);
                    await PublishVolumeAsync(force: false, token);
                    await Task.Delay(PollInterval, token);
                }
            }
            catch (OperationCanceledException)
            {
                return;
            }
            catch (Exception ex)
            {
                Status = $"Retrying: {ex.Message}";
            }

            if (token.IsCancellationRequested) return;
            try { await Task.Delay(ReconnectDelay, token); } catch (OperationCanceledException) { return; }
        }
    }

    private async Task ConnectAsync(CancellationToken token)
    {
        var options = OptionsStore.Load();
        if (string.IsNullOrWhiteSpace(options.Mqtt.Host))
            throw new InvalidOperationException("MQTT host is not configured yet.");

        _topics = new MqttTopics(options.DeviceId);
        _sessions ??= await GlobalSystemMediaTransportControlsSessionManager.RequestAsync();
        _volume ??= new SystemVolume();

        var client = new MqttFactory().CreateMqttClient();
        client.ApplicationMessageReceivedAsync += OnMessageAsync;

        var mqttOptions = new MqttClientOptionsBuilder()
            .WithClientId($"pcbot-media-{Environment.MachineName}")
            .WithTcpServer(options.Mqtt.Host, options.Mqtt.Port)
            .WithCredentials(options.Mqtt.Username, options.Mqtt.Password)
            .WithTlsOptions(tls => tls.UseTls())
            .WithCleanSession()
            .Build();

        await client.ConnectAsync(mqttOptions, token);
        await client.SubscribeAsync(_topics.VolumeCommand, MqttQualityOfServiceLevel.AtMostOnce, token);
        _client = client;

        // Announce the current level immediately so the bot's knob and the
        // screen agree from the first frame.
        await PublishVolumeAsync(force: true, token);
    }

    private async Task OnMessageAsync(MqttApplicationMessageReceivedEventArgs args)
    {
        if (_topics is null || _volume is null) return;
        if (!string.Equals(args.ApplicationMessage.Topic, _topics.VolumeCommand, StringComparison.Ordinal)) return;

        try
        {
            var payload = Encoding.UTF8.GetString(args.ApplicationMessage.PayloadSegment);
            var setpoint = JsonSerializer.Deserialize<VolumeSetpoint>(payload, Json);
            if (setpoint is null) return;
            _volume.SetLevel(setpoint.Level);
            await PublishVolumeAsync(force: true, _cancellation.Token);
        }
        catch (Exception ex)
        {
            Status = $"Volume command failed: {ex.Message}";
        }
    }

    private async Task PublishMediaAsync(CancellationToken token)
    {
        if (_client is not { IsConnected: true } || _topics is null || _sessions is null) return;

        var state = await ReadMediaAsync();
        Media = state;
        var payload = JsonSerializer.Serialize(state, Json);
        // Media only changes on track or transport changes, so skip the churn.
        if (payload == _lastMediaPayload) return;
        _lastMediaPayload = payload;
        Updated?.Invoke();
        await PublishAsync(_topics.MediaState, payload, retain: true,
            MqttQualityOfServiceLevel.AtLeastOnce, token);
    }

    private async Task<MediaState> ReadMediaAsync()
    {
        var now = DateTimeOffset.UtcNow;
        var session = _sessions?.GetCurrentSession();
        if (session is null)
            return new MediaState("stopped", "", "", "", "", null, null, now);

        var playback = session.GetPlaybackInfo();
        var status = playback.PlaybackStatus switch
        {
            GlobalSystemMediaTransportControlsSessionPlaybackStatus.Playing => "playing",
            GlobalSystemMediaTransportControlsSessionPlaybackStatus.Paused => "paused",
            _ => "stopped",
        };

        string title = "", artist = "", album = "";
        try
        {
            var properties = await session.TryGetMediaPropertiesAsync();
            title = properties.Title ?? "";
            artist = properties.Artist ?? "";
            album = properties.AlbumTitle ?? "";
        }
        catch
        {
            // Some players briefly refuse property reads while switching tracks.
        }

        double? position = null;
        double? duration = null;
        try
        {
            var timeline = session.GetTimelineProperties();
            position = timeline.Position.TotalSeconds;
            duration = timeline.EndTime.TotalSeconds;
            if (duration <= 0) { position = null; duration = null; }
        }
        catch
        {
            // Timeline is optional; the screen falls back to a bare progress bar.
        }

        var app = session.SourceAppUserModelId ?? "";
        return new MediaState(status, title, artist, album, FriendlyApp(app), position, duration, now);
    }

    /// <summary>Turns an app user model id into something worth putting on a 240 px screen.</summary>
    private static string FriendlyApp(string appUserModelId)
    {
        if (string.IsNullOrWhiteSpace(appUserModelId)) return "";
        var lowered = appUserModelId.ToLowerInvariant();
        if (lowered.Contains("spotify")) return "Spotify";
        if (lowered.Contains("chrome")) return "Chrome";
        if (lowered.Contains("msedge")) return "Edge";
        if (lowered.Contains("firefox")) return "Firefox";
        if (lowered.Contains("vlc")) return "VLC";
        if (lowered.Contains("zune") || lowered.Contains("media.player")) return "Media Player";
        var trimmed = appUserModelId.Split('!')[0];
        var lastSegment = trimmed.Split('\\', '.').LastOrDefault() ?? trimmed;
        return lastSegment.Length > 16 ? lastSegment[..16] : lastSegment;
    }

    private async Task PublishVolumeAsync(bool force, CancellationToken token)
    {
        if (_client is not { IsConnected: true } || _topics is null || _volume is null) return;

        var level = _volume.GetLevel();
        var muted = _volume.GetMuted();
        Volume = level;
        Muted = muted;
        if (!force && level == _lastPublishedLevel && muted == _lastPublishedMute) return;
        _lastPublishedLevel = level;
        _lastPublishedMute = muted;
        Updated?.Invoke();

        var payload = JsonSerializer.Serialize(
            new VolumeState(level, muted, DateTimeOffset.UtcNow), Json);
        await PublishAsync(_topics.VolumeState, payload, retain: true,
            MqttQualityOfServiceLevel.AtLeastOnce, token);
    }

    private async Task PublishAsync(string topic, string payload, bool retain,
        MqttQualityOfServiceLevel qos, CancellationToken token)
    {
        if (_client is not { IsConnected: true }) return;
        await _publishLock.WaitAsync(token);
        try
        {
            var message = new MqttApplicationMessageBuilder()
                .WithTopic(topic)
                .WithPayload(payload)
                .WithQualityOfServiceLevel(qos)
                .WithRetainFlag(retain)
                .Build();
            await _client.PublishAsync(message, token);
        }
        finally
        {
            _publishLock.Release();
        }
    }

    public async ValueTask DisposeAsync()
    {
        _cancellation.Cancel();
        if (_worker is not null)
        {
            try { await _worker; } catch (OperationCanceledException) { }
        }
        if (_client is not null)
        {
            if (_client.IsConnected) await _client.DisconnectAsync();
            _client.Dispose();
        }
        _volume?.Dispose();
        _cancellation.Dispose();
        _publishLock.Dispose();
    }
}

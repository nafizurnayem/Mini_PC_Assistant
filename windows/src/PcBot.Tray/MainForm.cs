using System.Text.Json;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using MQTTnet;
using MQTTnet.Client;
using MQTTnet.Protocol;
using PcBot.Core;

namespace PcBot.Tray;

public sealed class MainForm : Form
{
    private readonly TextBox _device = Field();
    private readonly TextBox _host = Field();
    private readonly NumericUpDown _port = new() { Minimum = 1, Maximum = 65535, Value = 8883, Dock = DockStyle.Fill };
    private readonly TextBox _username = Field();
    private readonly TextBox _password = Field(true);
    private readonly TextBox _speedTest = Field();
    private readonly NumericUpDown _telemetryInterval = Number(2, 300, 5);
    private readonly NumericUpDown _alertSustain = Number(1, 600, 15);
    private readonly NumericUpDown _alertCooldown = Number(10, 3600, 300);
    private readonly NumericUpDown _cpuTemp = Number(40, 120, 90);
    private readonly NumericUpDown _gpuTemp = Number(40, 120, 88);
    private readonly NumericUpDown _driveTemp = Number(30, 100, 70);
    private readonly NumericUpDown _memoryUsed = Number(50, 100, 95);
    private readonly NumericUpDown _driveFree = Number(1, 50, 10);
    private readonly NumericUpDown _hysteresis = Number(1, 20, 5);
    private readonly Label _status = new() { AutoSize = true, Text = "Ready" };
    private readonly Label _network = new() { AutoSize = true, MaximumSize = new Size(500, 0) };

    public MainForm()
    {
        Text = "PC Assistant Bot";
        Width = 560;
        Height = 760;
        MinimumSize = new Size(560, 620);
        var layout = new TableLayoutPanel {
            Dock = DockStyle.Fill,
            Padding = new Padding(16),
            ColumnCount = 2,
            AutoScroll = true,
        };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 150));
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        AddRow(layout, "Device ID", _device);
        AddRow(layout, "HiveMQ host", _host);
        AddRow(layout, "TLS port", _port);
        AddRow(layout, "Username", _username);
        AddRow(layout, "Password", _password);
        AddRow(layout, "Speedtest CLI", _speedTest);
        AddSection(layout, "Monitoring");
        AddRow(layout, "Telemetry interval (s)", _telemetryInterval);
        AddRow(layout, "Alert sustain (s)", _alertSustain);
        AddRow(layout, "Alert cooldown (s)", _alertCooldown);
        AddRow(layout, "CPU limit (°C)", _cpuTemp);
        AddRow(layout, "GPU limit (°C)", _gpuTemp);
        AddRow(layout, "Drive limit (°C)", _driveTemp);
        AddRow(layout, "RAM limit (%)", _memoryUsed);
        AddRow(layout, "Minimum drive free (%)", _driveFree);
        AddRow(layout, "Recovery hysteresis", _hysteresis);
        AddSection(layout, "LAN diagnostics");
        layout.Controls.Add(_network, 0, layout.RowCount);
        layout.SetColumnSpan(_network, 2);
        layout.RowCount++;

        var buttons = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true };
        buttons.Controls.Add(Button("Save", async (_, _) => await SaveAsync()));
        buttons.Controls.Add(Button("Test MQTT", async (_, _) => await TestMqttAsync()));
        buttons.Controls.Add(Button("Wi-Fi reset", async (_, _) => await ResetAsync("wifi")));
        buttons.Controls.Add(Button("Factory reset", async (_, _) => await ConfirmFactoryResetAsync()));
        layout.Controls.Add(buttons, 0, layout.RowCount);
        layout.SetColumnSpan(buttons, 2);
        layout.RowCount++;
        layout.Controls.Add(_status, 0, layout.RowCount);
        layout.SetColumnSpan(_status, 2);
        Controls.Add(layout);
        LoadOptions();
    }

    private void LoadOptions()
    {
        var options = OptionsStore.Load();
        _device.Text = options.DeviceId;
        _host.Text = options.Mqtt.Host;
        _port.Value = Math.Clamp(options.Mqtt.Port, 1, 65535);
        _username.Text = options.Mqtt.Username;
        _password.Text = options.Mqtt.Password;
        _speedTest.Text = options.SpeedTestExecutable;
        _telemetryInterval.Value = options.TelemetryIntervalSeconds;
        _alertSustain.Value = options.AlertSustainSeconds;
        _alertCooldown.Value = options.AlertCooldownSeconds;
        _cpuTemp.Value = (decimal)options.Thresholds.CpuTempC;
        _gpuTemp.Value = (decimal)options.Thresholds.GpuTempC;
        _driveTemp.Value = (decimal)options.Thresholds.DriveTempC;
        _memoryUsed.Value = (decimal)options.Thresholds.MemoryUsedPercent;
        _driveFree.Value = (decimal)options.Thresholds.DriveFreePercent;
        _hysteresis.Value = (decimal)options.Thresholds.Hysteresis;
        _network.Text = DetectLan();
    }

    private AgentOptions ReadOptions()
    {
        var current = OptionsStore.Load();
        current.DeviceId = _device.Text.Trim();
        current.Mqtt.Host = _host.Text.Trim();
        current.Mqtt.Port = (int)_port.Value;
        current.Mqtt.Username = _username.Text;
        current.Mqtt.Password = _password.Text;
        current.SpeedTestExecutable = _speedTest.Text.Trim();
        current.TelemetryIntervalSeconds = (int)_telemetryInterval.Value;
        current.AlertSustainSeconds = (int)_alertSustain.Value;
        current.AlertCooldownSeconds = (int)_alertCooldown.Value;
        current.Thresholds.CpuTempC = (double)_cpuTemp.Value;
        current.Thresholds.GpuTempC = (double)_gpuTemp.Value;
        current.Thresholds.DriveTempC = (double)_driveTemp.Value;
        current.Thresholds.MemoryUsedPercent = (double)_memoryUsed.Value;
        current.Thresholds.DriveFreePercent = (double)_driveFree.Value;
        current.Thresholds.Hysteresis = (double)_hysteresis.Value;
        return current;
    }

    private Task SaveAsync()
    {
        OptionsStore.Save(ReadOptions());
        _status.Text = $"Saved. Restart PC Assistant Bot service to apply changes.";
        return Task.CompletedTask;
    }

    private async Task TestMqttAsync()
    {
        _status.Text = "Connecting...";
        try {
            await using var session = await ConnectAsync(ReadOptions());
            _status.Text = "HiveMQ TLS connection successful.";
        } catch (Exception ex) {
            _status.Text = $"Connection failed: {ex.Message}";
        }
    }

    private async Task ResetAsync(string action)
    {
        var options = ReadOptions();
        try {
            await using var session = await ConnectAsync(options);
            var topics = new MqttTopics(options.DeviceId);
            var now = DateTimeOffset.UtcNow;
            var command = new CommandEnvelope(Guid.NewGuid().ToString(), "tray", action, now, now.AddSeconds(30));
            var message = new MqttApplicationMessageBuilder()
                .WithTopic(topics.ResetCommand)
                .WithPayload(JsonSerializer.Serialize(command, new JsonSerializerOptions(JsonSerializerDefaults.Web)))
                .WithQualityOfServiceLevel(MqttQualityOfServiceLevel.AtLeastOnce)
                .WithRetainFlag(false)
                .Build();
            await session.Client.PublishAsync(message);
            _status.Text = action == "wifi" ? "Wi-Fi reset sent." : "Factory reset sent.";
        } catch (Exception ex) {
            _status.Text = $"Reset failed: {ex.Message}";
        }
    }

    private async Task ConfirmFactoryResetAsync()
    {
        if (MessageBox.Show("Erase bot Wi-Fi, MQTT credentials, pairing, and settings?",
                "Factory reset", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.Yes)
            await ResetAsync("factory");
    }

    private static async Task<MqttSession> ConnectAsync(AgentOptions options)
    {
        var client = new MqttFactory().CreateMqttClient();
        var mqttOptions = new MqttClientOptionsBuilder()
            .WithClientId($"pcbot-tray-{Guid.NewGuid():N}")
            .WithTcpServer(options.Mqtt.Host, options.Mqtt.Port)
            .WithCredentials(options.Mqtt.Username, options.Mqtt.Password)
            .WithTlsOptions(tls => tls.UseTls())
            .WithCleanSession()
            .Build();
        await client.ConnectAsync(mqttOptions);
        return new MqttSession(client);
    }

    private static TextBox Field(bool password = false) => new()
    {
        Dock = DockStyle.Fill,
        UseSystemPasswordChar = password,
    };

    private static NumericUpDown Number(decimal minimum, decimal maximum, decimal value) => new()
    {
        Dock = DockStyle.Fill,
        Minimum = minimum,
        Maximum = maximum,
        Value = value,
        DecimalPlaces = 0,
    };

    private static Button Button(string text, EventHandler onClick)
    {
        var button = new Button { Text = text, AutoSize = true };
        button.Click += onClick;
        return button;
    }

    private static void AddRow(TableLayoutPanel layout, string label, Control control)
    {
        layout.Controls.Add(new Label { Text = label, AutoSize = true, Anchor = AnchorStyles.Left }, 0, layout.RowCount);
        layout.Controls.Add(control, 1, layout.RowCount);
        layout.RowCount++;
    }

    private static void AddSection(TableLayoutPanel layout, string label)
    {
        var heading = new Label {
            Text = label.ToUpperInvariant(),
            AutoSize = true,
            Font = new Font("Segoe UI", 9F, FontStyle.Bold),
            Padding = new Padding(0, 14, 0, 4),
        };
        layout.Controls.Add(heading, 0, layout.RowCount);
        layout.SetColumnSpan(heading, 2);
        layout.RowCount++;
    }

    private static string DetectLan()
    {
        foreach (var adapter in NetworkInterface.GetAllNetworkInterfaces().Where(value =>
                     value.OperationalStatus == OperationalStatus.Up &&
                     value.NetworkInterfaceType is not NetworkInterfaceType.Loopback and
                         not NetworkInterfaceType.Tunnel))
        {
            var address = adapter.GetIPProperties().UnicastAddresses.FirstOrDefault(value =>
                value.Address.AddressFamily == AddressFamily.InterNetwork && value.IPv4Mask is not null);
            if (address?.IPv4Mask is null) continue;
            var ip = address.Address.GetAddressBytes();
            var mask = address.IPv4Mask.GetAddressBytes();
            var broadcast = new byte[4];
            for (var index = 0; index < 4; index++)
                broadcast[index] = (byte)(ip[index] | ~mask[index]);
            var mac = string.Join(":", adapter.GetPhysicalAddress().GetAddressBytes()
                .Select(value => value.ToString("X2")));
            return $"{adapter.Name}: MAC {mac} · IP {address.Address} · Broadcast {new IPAddress(broadcast)}";
        }
        return "No active IPv4 LAN adapter detected.";
    }

    private sealed class MqttSession(IMqttClient client) : IAsyncDisposable
    {
        public IMqttClient Client { get; } = client;
        public async ValueTask DisposeAsync()
        {
            if (Client.IsConnected) await Client.DisconnectAsync();
            Client.Dispose();
        }
    }
}

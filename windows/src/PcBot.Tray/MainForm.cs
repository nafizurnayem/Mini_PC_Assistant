using System.Text.Json;
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
    private readonly Label _status = new() { AutoSize = true, Text = "Ready" };

    public MainForm()
    {
        Text = "PC Assistant Bot";
        Width = 560;
        Height = 430;
        MinimumSize = new Size(520, 400);
        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(16), ColumnCount = 2 };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 150));
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        AddRow(layout, "Device ID", _device);
        AddRow(layout, "HiveMQ host", _host);
        AddRow(layout, "TLS port", _port);
        AddRow(layout, "Username", _username);
        AddRow(layout, "Password", _password);
        AddRow(layout, "Speedtest CLI", _speedTest);

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


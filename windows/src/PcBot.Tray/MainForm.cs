using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Text.Json;
using MQTTnet;
using MQTTnet.Client;
using MQTTnet.Protocol;
using PcBot.Core;

namespace PcBot.Tray;

/// <summary>
/// Desktop companion for Pico Assistant: a live dashboard first, settings
/// behind it, and a background agent that keeps publishing while the window is
/// closed to the notification area.
///
/// Every page is built from auto-sizing tables so the layout holds together at
/// any display scaling and with any text length.
/// </summary>
public sealed class MainForm : Form
{
    private readonly TextBox _device = UiTheme.Field();
    private readonly TextBox _host = UiTheme.Field();
    private readonly NumericUpDown _port = UiTheme.Number(1, 65535, 8883);
    private readonly TextBox _username = UiTheme.Field();
    private readonly TextBox _password = UiTheme.Field(true);
    private readonly TextBox _speedTest = UiTheme.Field();
    private readonly NumericUpDown _telemetryInterval = UiTheme.Number(2, 300, 5);
    private readonly NumericUpDown _alertSustain = UiTheme.Number(1, 600, 15);
    private readonly NumericUpDown _alertCooldown = UiTheme.Number(10, 3600, 300);
    private readonly NumericUpDown _cpuTemp = UiTheme.Number(40, 120, 90);
    private readonly NumericUpDown _gpuTemp = UiTheme.Number(40, 120, 88);
    private readonly NumericUpDown _driveTemp = UiTheme.Number(30, 100, 70);
    private readonly NumericUpDown _memoryUsed = UiTheme.Number(50, 100, 95);
    private readonly NumericUpDown _driveFree = UiTheme.Number(1, 50, 10);
    private readonly NumericUpDown _hysteresis = UiTheme.Number(1, 20, 5);

    private readonly Label _status = new()
    {
        AutoSize = true, Text = "Ready", ForeColor = UiTheme.TextSoft, Font = UiTheme.Body,
        Margin = new Padding(0),
    };
    private readonly Label _network = new()
    {
        AutoSize = true, ForeColor = UiTheme.TextSoft, Font = UiTheme.Body,
        Margin = new Padding(0), Dock = DockStyle.Top,
    };

    private readonly StatusPill _brokerPill = new();
    private readonly StatusPill _mediaPill = new();

    private readonly Label _trackTitle = new()
    {
        AutoSize = true, ForeColor = UiTheme.Text, Font = UiTheme.Display,
        Text = "Nothing playing", Margin = new Padding(0, 0, 0, 2), Dock = DockStyle.Top,
    };
    private readonly Label _trackArtist = new()
    {
        AutoSize = true, ForeColor = UiTheme.TextSoft, Font = UiTheme.Body, Text = "—",
        Margin = new Padding(0, 0, 0, 10), Dock = DockStyle.Top,
    };
    private readonly LevelBar _trackProgress = new();
    private readonly Label _trackTime = new()
    {
        AutoSize = true, ForeColor = UiTheme.Muted, Font = UiTheme.Small, Text = "",
        Margin = new Padding(0), Dock = DockStyle.Top,
    };

    private readonly Label _volumeValue = new()
    {
        AutoSize = true, ForeColor = UiTheme.Text, Font = UiTheme.Value, Text = "—",
        Margin = new Padding(0), Dock = DockStyle.Top,
    };
    private readonly SliderBar _volumeSlider = new();
    private readonly Label _volumeHint = UiTheme.Caption("Drag to set the PC's master volume. The bot's knob moves this too.");

    private readonly Panel _pageHost = new()
    {
        Dock = DockStyle.Fill, BackColor = UiTheme.Ink, AutoScroll = true, Padding = new Padding(20, 18, 20, 18),
    };
    private readonly List<Button> _navButtons = new();
    private readonly Label _firmwareDevice = new()
    {
        AutoSize = true, ForeColor = UiTheme.TextSoft, Font = UiTheme.Body,
        Margin = new Padding(0, 0, 0, 6), Dock = DockStyle.Top, Text = "Waiting for the device...",
    };
    private readonly Label _firmwareFile = new()
    {
        AutoSize = true, ForeColor = UiTheme.Text, Font = UiTheme.Body,
        Margin = new Padding(0, 0, 0, 8), Dock = DockStyle.Top, Text = "No image selected",
    };
    private readonly LevelBar _firmwareProgress = new();
    private readonly FirmwareUpdater _updater = new();
    private string _firmwarePath = "";
    private bool _uploading;

    private readonly NotifyIcon _tray = new();
    private readonly MediaVolumeAgent _agent = new();
    private readonly System.Windows.Forms.Timer _refresh = new() { Interval = 700 };
    private bool _exiting;
    private DateTime _lastVolumeSend = DateTime.MinValue;

    public MainForm()
    {
        Text = "Pico Assistant";
        ClientSize = new Size(960, 720);
        MinimumSize = new Size(820, 620);
        BackColor = UiTheme.Ink;
        ForeColor = UiTheme.Text;
        Font = UiTheme.Body;
        AutoScaleMode = AutoScaleMode.Font;

        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 2, BackColor = UiTheme.Ink,
        };
        root.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));

        root.Controls.Add(BuildNav(), 0, 0);
        root.Controls.Add(_pageHost, 1, 0);

        var footer = new Panel
        {
            Dock = DockStyle.Fill, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink,
            BackColor = UiTheme.Backdrop, Padding = new Padding(20, 12, 20, 12),
        };
        _status.Dock = DockStyle.Top;
        footer.Controls.Add(_status);
        root.Controls.Add(footer, 0, 1);
        root.SetColumnSpan(footer, 2);

        Controls.Add(root);
        LoadOptions();
        ShowPage(0);
        StartBackgroundAgent();
    }

    private Control BuildNav()
    {
        var nav = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, ColumnCount = 1, AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink, BackColor = UiTheme.Backdrop,
            Padding = new Padding(14, 20, 14, 14), GrowStyle = TableLayoutPanelGrowStyle.AddRows,
        };
        nav.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));

        nav.Controls.Add(new Label
        {
            Text = "PICO", AutoSize = true, ForeColor = UiTheme.Text, Font = UiTheme.Brand,
            Margin = new Padding(6, 0, 6, 0),
        });
        nav.Controls.Add(new Label
        {
            Text = "ASSISTANT", AutoSize = true, ForeColor = UiTheme.Accent, Font = UiTheme.BrandSub,
            Margin = new Padding(7, 0, 6, 20),
        });

        var titles = new[] { "Dashboard", "Connection", "Monitoring", "Firmware", "Device" };
        var width = titles.Max(title => TextRenderer.MeasureText(title, UiTheme.Body).Width) + 60;
        for (var index = 0; index < titles.Length; index++)
        {
            var page = index;
            var button = new Button
            {
                Text = titles[index],
                Width = width,
                Height = TextRenderer.MeasureText(titles[index], UiTheme.Body).Height + 20,
                FlatStyle = FlatStyle.Flat,
                TextAlign = ContentAlignment.MiddleLeft,
                Padding = new Padding(14, 0, 0, 0),
                ForeColor = UiTheme.TextSoft,
                BackColor = UiTheme.Backdrop,
                Font = UiTheme.Body,
                Cursor = Cursors.Hand,
                Margin = new Padding(0, 0, 0, 4),
                UseVisualStyleBackColor = false,
            };
            button.FlatAppearance.BorderSize = 0;
            button.FlatAppearance.MouseOverBackColor = UiTheme.Card;
            button.Click += (_, _) => ShowPage(page);
            _navButtons.Add(button);
            nav.Controls.Add(button);
        }
        return nav;
    }

    private void ShowPage(int index)
    {
        for (var i = 0; i < _navButtons.Count; i++)
        {
            var selected = i == index;
            _navButtons[i].BackColor = selected ? UiTheme.Card : UiTheme.Backdrop;
            _navButtons[i].ForeColor = selected ? UiTheme.Text : UiTheme.TextSoft;
        }

        _pageHost.SuspendLayout();
        _pageHost.Controls.Clear();
        var page = index switch
        {
            0 => BuildDashboard(),
            1 => BuildConnection(),
            2 => BuildMonitoring(),
            3 => BuildFirmware(),
            _ => BuildDevice(),
        };
        _pageHost.Controls.Add(page);
        _pageHost.ResumeLayout(true);
        RefreshLiveState();
    }

    /// <summary>Single-column stack that grows with its cards.</summary>
    private static TableLayoutPanel Page()
    {
        var page = new TableLayoutPanel
        {
            Dock = DockStyle.Top, ColumnCount = 1, AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink, BackColor = UiTheme.Ink,
            GrowStyle = TableLayoutPanelGrowStyle.AddRows,
        };
        page.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        return page;
    }

    private static TableLayoutPanel Grid()
    {
        var grid = new TableLayoutPanel
        {
            Dock = DockStyle.Top, ColumnCount = 2, AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink, BackColor = UiTheme.Backdrop,
            GrowStyle = TableLayoutPanelGrowStyle.AddRows, Margin = new Padding(0),
        };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        return grid;
    }

    private static FlowLayoutPanel Row()
    {
        return new FlowLayoutPanel
        {
            Dock = DockStyle.Top, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink,
            BackColor = UiTheme.Backdrop, Margin = new Padding(0, 4, 0, 0), WrapContents = true,
        };
    }

    private Control BuildDashboard()
    {
        var page = Page();

        var live = new CardPanel("Live status", UiTheme.Accent);
        var pills = Row();
        pills.Controls.Add(_brokerPill);
        pills.Controls.Add(_mediaPill);
        live.Controls.Add(pills);

        var media = new CardPanel("Now playing", UiTheme.Success);
        media.Controls.Add(_trackTitle);
        media.Controls.Add(_trackArtist);
        media.Controls.Add(_trackProgress);
        media.Controls.Add(_trackTime);

        var volume = new CardPanel("PC volume", UiTheme.Highlight);
        volume.Controls.Add(_volumeValue);
        volume.Controls.Add(_volumeSlider);
        volume.Controls.Add(_volumeHint);

        var actions = new CardPanel("Quick actions", UiTheme.AccentEnd);
        var row = Row();
        row.Controls.Add(UiTheme.PrimaryButton("Test broker", async (_, _) => await TestMqttAsync()));
        row.Controls.Add(UiTheme.GhostButton("Mute", async (_, _) => await SetVolumeAsync(0)));
        row.Controls.Add(UiTheme.GhostButton("Set 50%", async (_, _) => await SetVolumeAsync(50)));
        actions.Controls.Add(row);

        page.Controls.Add(live);
        page.Controls.Add(media);
        page.Controls.Add(volume);
        page.Controls.Add(actions);
        return page;
    }

    private Control BuildConnection()
    {
        var page = Page();

        var card = new CardPanel("Broker and identity", UiTheme.Accent);
        var grid = Grid();
        AddRow(grid, "Device ID", _device);
        AddRow(grid, "Broker host", _host);
        AddRow(grid, "TLS port", _port);
        AddRow(grid, "Username", _username);
        AddRow(grid, "Password", _password);
        card.Controls.Add(grid);
        card.Controls.Add(UiTheme.Caption(
            "The device, this PC, the phone app and Home Assistant each need their own broker credential."));

        var actions = new CardPanel();
        var row = Row();
        row.Controls.Add(UiTheme.PrimaryButton("Save settings", async (_, _) => await SaveAsync()));
        row.Controls.Add(UiTheme.GhostButton("Test connection", async (_, _) => await TestMqttAsync()));
        actions.Controls.Add(row);

        page.Controls.Add(card);
        page.Controls.Add(actions);
        return page;
    }

    private Control BuildMonitoring()
    {
        var page = Page();

        var sampling = new CardPanel("Sampling", UiTheme.AccentEnd);
        var samplingGrid = Grid();
        AddRow(samplingGrid, "Telemetry interval (s)", _telemetryInterval);
        AddRow(samplingGrid, "Alert sustain (s)", _alertSustain);
        AddRow(samplingGrid, "Alert cooldown (s)", _alertCooldown);
        AddRow(samplingGrid, "Speedtest CLI", _speedTest);
        sampling.Controls.Add(samplingGrid);

        var limits = new CardPanel("Alert thresholds", UiTheme.Warning);
        var limitGrid = Grid();
        AddRow(limitGrid, "CPU limit (°C)", _cpuTemp);
        AddRow(limitGrid, "GPU limit (°C)", _gpuTemp);
        AddRow(limitGrid, "Drive limit (°C)", _driveTemp);
        AddRow(limitGrid, "RAM limit (%)", _memoryUsed);
        AddRow(limitGrid, "Minimum drive free (%)", _driveFree);
        AddRow(limitGrid, "Recovery hysteresis", _hysteresis);
        limits.Controls.Add(limitGrid);
        limits.Controls.Add(UiTheme.Caption(
            "An alert fires once a limit is held for the sustain time, and clears once the value drops by the hysteresis."));

        var actions = new CardPanel();
        var row = Row();
        row.Controls.Add(UiTheme.PrimaryButton("Save settings", async (_, _) => await SaveAsync()));
        actions.Controls.Add(row);

        page.Controls.Add(sampling);
        page.Controls.Add(limits);
        page.Controls.Add(actions);
        return page;
    }

    private Control BuildFirmware()
    {
        var page = Page();

        var device = new CardPanel("Device", UiTheme.Accent);
        device.Controls.Add(_firmwareDevice);
        device.Controls.Add(UiTheme.Caption(
            "Address and update token come from the device's retained status message, so there is nothing to type in."));

        var update = new CardPanel("Install firmware over Wi-Fi", UiTheme.AccentEnd);
        update.Controls.Add(_firmwareFile);
        update.Controls.Add(_firmwareProgress);
        var row = Row();
        row.Controls.Add(UiTheme.GhostButton("Choose image...", (_, _) => ChooseFirmware()));
        row.Controls.Add(UiTheme.PrimaryButton("Install", async (_, _) => await UploadFirmwareAsync()));
        update.Controls.Add(row);
        update.Controls.Add(UiTheme.Caption(
            "Built images live in firmware\\.pio\\build\\pcbot-esp32-s3-n4r2\\firmware.bin. " +
            "The device keeps running its current firmware until the new image is fully received and verified."));

        page.Controls.Add(device);
        page.Controls.Add(update);
        return page;
    }

    private void ChooseFirmware()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "Select firmware image",
            Filter = "Firmware image (*.bin)|*.bin|All files (*.*)|*.*",
            InitialDirectory = FindFirmwareDirectory(),
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        _firmwarePath = dialog.FileName;
        var size = new FileInfo(_firmwarePath).Length / 1024.0;
        _firmwareFile.Text = $"{Path.GetFileName(_firmwarePath)}   ·   {size:0} KB";
    }

    /// <summary>Best guess at the PlatformIO output folder, relative to the repo.</summary>
    private static string FindFirmwareDirectory()
    {
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        while (directory is not null)
        {
            var candidate = Path.Combine(directory.FullName, "firmware", ".pio", "build",
                "pcbot-esp32-s3-n4r2");
            if (Directory.Exists(candidate)) return candidate;
            directory = directory.Parent;
        }
        return Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
    }

    private async Task UploadFirmwareAsync()
    {
        if (string.IsNullOrEmpty(_firmwarePath))
        {
            _status.Text = "Choose a firmware image first.";
            return;
        }
        if (string.IsNullOrWhiteSpace(_agent.DeviceIp) || string.IsNullOrWhiteSpace(_agent.OtaToken))
        {
            _status.Text = "Waiting for the device to report its address. Is it online?";
            return;
        }
        if (_uploading) return;

        _uploading = true;
        _firmwareProgress.Value = 0;
        _status.Text = "Uploading firmware...";
        try
        {
            var progress = new Progress<int>(percent =>
            {
                _firmwareProgress.Value = percent;
                _status.Text = $"Uploading firmware... {percent}%";
            });
            await _updater.UploadAsync(_agent.DeviceIp, _agent.OtaToken, _firmwarePath,
                progress, CancellationToken.None);
            _firmwareProgress.Value = 100;
            _status.Text = "Image accepted. The device is rebooting into it.";
        }
        catch (Exception ex)
        {
            _status.Text = $"Update failed: {ex.Message}";
        }
        finally
        {
            _uploading = false;
        }
    }

    private Control BuildDevice()
    {
        var page = Page();

        var lan = new CardPanel("LAN diagnostics", UiTheme.Highlight);
        lan.Controls.Add(_network);
        lan.Controls.Add(UiTheme.Caption("Use these values for the device's MAC and broadcast fields during setup."));

        var danger = new CardPanel("Reset the device", UiTheme.Danger);
        danger.Controls.Add(new Label
        {
            AutoSize = true, Dock = DockStyle.Top, ForeColor = UiTheme.TextSoft, Font = UiTheme.Body,
            Margin = new Padding(0, 0, 0, 10),
            Text = "Wi-Fi reset clears only the network and reopens setup.\r\n" +
                   "Factory reset erases every stored credential and pairing.",
        });
        var row = Row();
        row.Controls.Add(UiTheme.GhostButton("Wi-Fi reset", async (_, _) => await ResetAsync("wifi")));
        row.Controls.Add(UiTheme.DestructiveButton("Factory reset", async (_, _) => await ConfirmFactoryResetAsync()));
        danger.Controls.Add(row);

        page.Controls.Add(lan);
        page.Controls.Add(danger);
        return page;
    }

    private void StartBackgroundAgent()
    {
        _tray.Icon = SystemIcons.Application;
        _tray.Text = "Pico Assistant";
        _tray.Visible = true;
        _tray.DoubleClick += (_, _) => RestoreWindow();

        var menu = new ContextMenuStrip();
        menu.Items.Add("Open", null, (_, _) => RestoreWindow());
        menu.Items.Add("Quit", null, (_, _) => { _exiting = true; Close(); });
        _tray.ContextMenuStrip = menu;

        _volumeSlider.ValueChanging += OnVolumeDragging;
        _volumeSlider.ValueCommitted += OnVolumeCommitted;
        _refresh.Tick += (_, _) => RefreshLiveState();
        _agent.Start();
        _refresh.Start();
    }

    private void RefreshLiveState()
    {
        _brokerPill.Set(_agent.Connected ? "Broker connected" : _agent.Status,
            _agent.Connected ? UiTheme.Success : UiTheme.Warning);

        var media = _agent.Media;
        var playing = media is { Status: "playing" };
        _mediaPill.Set(playing ? "Playing" : media is { Status: "paused" } ? "Paused" : "No media",
            playing ? UiTheme.Success : UiTheme.Muted);

        if (media is not null && !string.IsNullOrWhiteSpace(media.Title))
        {
            _trackTitle.Text = media.Title;
            _trackArtist.Text = string.IsNullOrWhiteSpace(media.Artist)
                ? media.App
                : $"{media.Artist}     {media.App}";
            if (media.PositionSeconds is { } position && media.DurationSeconds is { } duration && duration > 0)
            {
                _trackProgress.Value = (int)Math.Clamp(position / duration * 100, 0, 100);
                _trackTime.Text = $"{Format(position)} / {Format(duration)}";
            }
            else
            {
                _trackProgress.Value = 0;
                _trackTime.Text = "Live";
            }
        }
        else
        {
            _trackTitle.Text = "Nothing playing";
            _trackArtist.Text = "—";
            _trackProgress.Value = 0;
            _trackTime.Text = "";
        }

        _firmwareDevice.Text = string.IsNullOrWhiteSpace(_agent.DeviceIp)
            ? "Waiting for the device to report in..."
            : $"Firmware {_agent.DeviceVersion}   at   {_agent.DeviceIp}";

        _volumeValue.Text = _agent.Muted ? "Muted" : $"{_agent.Volume}%";
        // Never fight the user's hand: skip the sync while the knob is held.
        if (!_volumeSlider.Dragging) _volumeSlider.Value = Math.Clamp(_agent.Volume, 0, 100);
    }

    private static string Format(double seconds)
    {
        var span = TimeSpan.FromSeconds(Math.Max(seconds, 0));
        return $"{(int)span.TotalMinutes}:{span.Seconds:00}";
    }

    private async void OnVolumeDragging(int level)
    {
        _volumeValue.Text = $"{level}%";
        // Throttle while dragging so a sweep does not fire a hundred writes.
        if ((DateTime.UtcNow - _lastVolumeSend).TotalMilliseconds < 120) return;
        _lastVolumeSend = DateTime.UtcNow;
        await SetVolumeAsync(level, quiet: true);
    }

    private async void OnVolumeCommitted(int level) => await SetVolumeAsync(level);

    private async Task SetVolumeAsync(int level, bool quiet = false)
    {
        try
        {
            await _agent.SetVolumeAsync(level);
            if (!quiet) _status.Text = $"Volume set to {level}%.";
        }
        catch (Exception ex)
        {
            _status.Text = $"Volume change failed: {ex.Message}";
        }
    }

    private void RestoreWindow()
    {
        Show();
        WindowState = FormWindowState.Normal;
        Activate();
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        // Now-playing and volume run for as long as the user is logged in, so
        // closing the window only hides it.
        if (!_exiting && e.CloseReason == CloseReason.UserClosing)
        {
            e.Cancel = true;
            Hide();
            return;
        }
        _refresh.Stop();
        _tray.Visible = false;
        _agent.DisposeAsync().AsTask().GetAwaiter().GetResult();
        base.OnFormClosing(e);
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
        var options = ReadOptions();
        if (string.IsNullOrWhiteSpace(options.DeviceId))
        {
            _status.Text = "Device ID is required.";
            return Task.CompletedTask;
        }
        if (string.IsNullOrWhiteSpace(options.Mqtt.Host))
        {
            _status.Text = "Broker host is required.";
            return Task.CompletedTask;
        }
        OptionsStore.Save(options);
        _status.Text = "Saved. Restart the Pico Assistant service to apply changes.";
        return Task.CompletedTask;
    }

    private async Task TestMqttAsync()
    {
        _status.Text = "Connecting...";
        try
        {
            await using var session = await ConnectAsync(ReadOptions());
            _status.Text = "Broker TLS connection successful.";
        }
        catch (Exception ex)
        {
            _status.Text = $"Connection failed: {ex.Message}";
        }
    }

    private async Task ResetAsync(string action)
    {
        var options = ReadOptions();
        try
        {
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
        }
        catch (Exception ex)
        {
            _status.Text = $"Reset failed: {ex.Message}";
        }
    }

    private async Task ConfirmFactoryResetAsync()
    {
        if (MessageBox.Show("Erase device Wi-Fi, MQTT credentials, pairing, and settings?",
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

    private static void AddRow(TableLayoutPanel layout, string label, Control control)
    {
        layout.Controls.Add(UiTheme.FieldLabel(label), 0, layout.RowCount);
        layout.Controls.Add(control, 1, layout.RowCount);
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
            return $"Adapter\t{adapter.Name}\r\nMAC\t{mac}\r\nIP\t{address.Address}\r\nBroadcast\t{new IPAddress(broadcast)}";
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

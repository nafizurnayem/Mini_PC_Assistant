using System.Drawing.Drawing2D;

namespace PcBot.Tray;

/// <summary>
/// Rounded titled surface. It is a single-column table so children stack in the
/// order they are added and the card grows to fit them, instead of being pinned
/// to a hand-guessed pixel height.
/// </summary>
public sealed class CardPanel : TableLayoutPanel
{
    private readonly string _title;
    private readonly Color? _accent;

    public CardPanel(string title = "", Color? accent = null)
    {
        _title = title;
        _accent = accent;
        ColumnCount = 1;
        GrowStyle = TableLayoutPanelGrowStyle.AddRows;
        AutoSize = true;
        AutoSizeMode = AutoSizeMode.GrowAndShrink;
        Dock = DockStyle.Top;
        DoubleBuffered = true;
        BackColor = UiTheme.Backdrop;
        ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        var titleHeight = string.IsNullOrEmpty(title)
            ? 16
            : TextRenderer.MeasureText(title, UiTheme.Heading).Height + 26;
        Padding = new Padding(16, titleHeight, 16, 16);
        Margin = new Padding(0, 0, 0, 12);
    }

    protected override void OnPaintBackground(PaintEventArgs e)
    {
        e.Graphics.Clear(UiTheme.Ink);
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        var bounds = new Rectangle(0, 0, Width - 1, Height - 1);
        using var path = UiTheme.RoundedRect(bounds, 10);
        using var body = new LinearGradientBrush(
            new Rectangle(0, 0, Math.Max(Width, 1), Math.Max(Height, 1)),
            UiTheme.Card, UiTheme.Backdrop, 90F);
        e.Graphics.FillPath(body, path);
        using var pen = new Pen(_accent is null
            ? UiTheme.Border
            : UiTheme.Blend(UiTheme.Border, _accent.Value, 0.55f));
        e.Graphics.DrawPath(pen, path);

        if (string.IsNullOrEmpty(_title)) return;
        var textTop = 15;
        if (_accent is { } accent)
        {
            using var marker = new SolidBrush(accent);
            var height = TextRenderer.MeasureText(_title, UiTheme.Heading).Height;
            e.Graphics.FillPath(marker,
                UiTheme.RoundedRect(new Rectangle(16, textTop + 1, 4, height - 2), 2));
        }
        TextRenderer.DrawText(e.Graphics, _title.ToUpperInvariant(), UiTheme.Heading,
            new Point(_accent is null ? 16 : 27, textTop), UiTheme.TextSoft);
    }
}

/// <summary>Status chip: a coloured dot plus a word, sized to its text.</summary>
public sealed class StatusPill : Control
{
    private Color _tone = UiTheme.Muted;

    public StatusPill()
    {
        DoubleBuffered = true;
        Font = UiTheme.Body;
        Margin = new Padding(0, 0, 10, 0);
        Set("Unknown", UiTheme.Muted);
    }

    public void Set(string text, Color tone)
    {
        if (Text == text && _tone == tone) return;
        Text = text;
        _tone = tone;
        var measured = TextRenderer.MeasureText(text, Font);
        Size = new Size(measured.Width + 42, measured.Height + 14);
        Invalidate();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.Clear(Parent?.BackColor ?? UiTheme.Backdrop);
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        var bounds = new Rectangle(0, 0, Width - 1, Height - 1);
        using var path = UiTheme.RoundedRect(bounds, Height / 2);
        using var fill = new SolidBrush(UiTheme.Blend(UiTheme.Card, _tone, 0.20f));
        e.Graphics.FillPath(fill, path);
        using var pen = new Pen(UiTheme.Blend(UiTheme.Border, _tone, 0.6f));
        e.Graphics.DrawPath(pen, path);
        using var dot = new SolidBrush(_tone);
        e.Graphics.FillEllipse(dot, 12, Height / 2 - 4, 8, 8);
        TextRenderer.DrawText(e.Graphics, Text, Font,
            new Rectangle(26, 0, Width - 30, Height), UiTheme.Text,
            TextFormatFlags.VerticalCenter | TextFormatFlags.Left);
    }
}

/// <summary>Read-only meter with a gradient fill.</summary>
public sealed class LevelBar : Control
{
    private int _value;

    public LevelBar()
    {
        DoubleBuffered = true;
        Height = 8;
        Dock = DockStyle.Top;
        Margin = new Padding(0, 6, 0, 6);
    }

    public int Value
    {
        get => _value;
        set
        {
            var clamped = Math.Clamp(value, 0, 100);
            if (clamped == _value) return;
            _value = clamped;
            Invalidate();
        }
    }

    public Color From { get; set; } = UiTheme.Accent;
    public Color To { get; set; } = UiTheme.Highlight;

    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.Clear(Parent?.BackColor ?? UiTheme.Backdrop);
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        var track = new Rectangle(0, 0, Width - 1, Height - 1);
        using var trackPath = UiTheme.RoundedRect(track, Height / 2);
        using var trackBrush = new SolidBrush(UiTheme.Ink);
        e.Graphics.FillPath(trackBrush, trackPath);

        var filled = (int)((Width - 1) * (_value / 100f));
        if (filled < 3) return;
        using var fillPath = UiTheme.RoundedRect(new Rectangle(0, 0, filled, Height - 1), Height / 2);
        using var brush = new LinearGradientBrush(
            new Rectangle(0, 0, Math.Max(filled, 2), Height), From, To, 0F);
        e.Graphics.FillPath(brush, fillPath);
    }
}

/// <summary>
/// Draggable slider. Replaces TrackBar, which ignores theming and paints system
/// chrome on a dark surface. Reports live changes while dragging and a separate
/// committed value on release, so callers can avoid flooding the broker.
/// </summary>
public sealed class SliderBar : Control
{
    private const int TrackHeight = 8;
    private const int KnobRadius = 9;
    private int _value;
    private bool _dragging;

    public SliderBar()
    {
        DoubleBuffered = true;
        Height = KnobRadius * 2 + 10;
        Dock = DockStyle.Top;
        Margin = new Padding(0, 8, 0, 4);
        Cursor = Cursors.Hand;
    }

    public event Action<int>? ValueChanging;
    public event Action<int>? ValueCommitted;

    public int Value
    {
        get => _value;
        set
        {
            var clamped = Math.Clamp(value, 0, 100);
            if (clamped == _value) return;
            _value = clamped;
            Invalidate();
        }
    }

    /// <summary>True while the user is holding the knob, so live updates can be ignored.</summary>
    public bool Dragging => _dragging;

    private void ApplyFromMouse(int x)
    {
        var span = Math.Max(Width - KnobRadius * 2, 1);
        var next = Math.Clamp((int)Math.Round((x - KnobRadius) * 100.0 / span), 0, 100);
        if (next == _value) return;
        _value = next;
        Invalidate();
        ValueChanging?.Invoke(next);
    }

    protected override void OnMouseDown(MouseEventArgs e)
    {
        _dragging = true;
        ApplyFromMouse(e.X);
        base.OnMouseDown(e);
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        if (_dragging) ApplyFromMouse(e.X);
        base.OnMouseMove(e);
    }

    protected override void OnMouseUp(MouseEventArgs e)
    {
        if (_dragging)
        {
            _dragging = false;
            ValueCommitted?.Invoke(_value);
        }
        base.OnMouseUp(e);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.Clear(Parent?.BackColor ?? UiTheme.Backdrop);
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;

        var top = (Height - TrackHeight) / 2;
        var track = new Rectangle(KnobRadius, top, Math.Max(Width - KnobRadius * 2 - 1, 1), TrackHeight);
        using var trackPath = UiTheme.RoundedRect(track, TrackHeight / 2);
        using var trackBrush = new SolidBrush(UiTheme.Ink);
        e.Graphics.FillPath(trackBrush, trackPath);

        var filled = (int)(track.Width * (_value / 100f));
        if (filled > 2)
        {
            using var fillPath = UiTheme.RoundedRect(
                new Rectangle(track.X, track.Y, filled, track.Height), TrackHeight / 2);
            using var brush = new LinearGradientBrush(track, UiTheme.Accent, UiTheme.Highlight, 0F);
            e.Graphics.FillPath(brush, fillPath);
        }

        var knobX = track.X + filled;
        using var knobShadow = new SolidBrush(UiTheme.Ink);
        e.Graphics.FillEllipse(knobShadow, knobX - KnobRadius, Height / 2 - KnobRadius + 1,
            KnobRadius * 2, KnobRadius * 2);
        using var knob = new SolidBrush(_dragging ? UiTheme.Highlight : UiTheme.Text);
        e.Graphics.FillEllipse(knob, knobX - KnobRadius + 2, Height / 2 - KnobRadius + 2,
            KnobRadius * 2 - 4, KnobRadius * 2 - 4);
    }
}

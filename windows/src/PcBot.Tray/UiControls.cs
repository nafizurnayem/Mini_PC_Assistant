using System.Drawing.Drawing2D;

namespace PcBot.Tray;

/// <summary>Rounded, titled surface every page is built from.</summary>
public sealed class CardPanel : Panel
{
    private readonly string _title;
    private readonly Color? _accent;

    public CardPanel(string title, Color? accent = null)
    {
        _title = title;
        _accent = accent;
        DoubleBuffered = true;
        BackColor = UiTheme.Backdrop;
        Padding = new Padding(16, string.IsNullOrEmpty(title) ? 16 : 42, 16, 16);
        Margin = new Padding(0, 0, 0, 12);
    }

    protected override void OnPaintBackground(PaintEventArgs e)
    {
        e.Graphics.Clear(UiTheme.Backdrop);
        var bounds = new Rectangle(0, 0, Width - 1, Height - 1);
        UiTheme.FillCard(e.Graphics, bounds, _accent);
        if (string.IsNullOrEmpty(_title)) return;

        if (_accent is { } accent)
        {
            using var marker = new SolidBrush(accent);
            e.Graphics.FillPath(marker, UiTheme.RoundedRect(new Rectangle(16, 17, 4, 14), 2));
        }
        TextRenderer.DrawText(e.Graphics, _title.ToUpperInvariant(), UiTheme.Heading,
            new Point(_accent is null ? 16 : 26, 14), UiTheme.TextSoft);
    }
}

/// <summary>Coloured status chip: a dot plus a word.</summary>
public sealed class StatusPill : Control
{
    private Color _tone = UiTheme.Muted;

    public StatusPill()
    {
        DoubleBuffered = true;
        Height = 26;
        Width = 130;
        Font = UiTheme.Body;
        Text = "Unknown";
    }

    public void Set(string text, Color tone)
    {
        Text = text;
        _tone = tone;
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
        e.Graphics.FillEllipse(dot, 9, Height / 2 - 4, 8, 8);
        TextRenderer.DrawText(e.Graphics, Text, Font,
            new Rectangle(24, 0, Width - 28, Height), UiTheme.Text,
            TextFormatFlags.VerticalCenter | TextFormatFlags.Left);
    }
}

/// <summary>Horizontal meter with a gradient fill, used for volume and levels.</summary>
public sealed class LevelBar : Control
{
    private int _value;

    public LevelBar()
    {
        DoubleBuffered = true;
        Height = 10;
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

    public Color From { get; set; } = UiTheme.Primary;
    public Color To { get; set; } = UiTheme.Tertiary;

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
        var fill = new Rectangle(0, 0, filled, Height - 1);
        using var fillPath = UiTheme.RoundedRect(fill, Height / 2);
        using var brush = new LinearGradientBrush(
            new Rectangle(0, 0, Math.Max(filled, 2), Height), From, To, 0F);
        e.Graphics.FillPath(brush, fillPath);
    }
}

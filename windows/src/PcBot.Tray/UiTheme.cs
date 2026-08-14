using System.Drawing.Drawing2D;

namespace PcBot.Tray;

/// <summary>
/// Dark theme shared by every control in the desktop app, matching the palette
/// the device itself runs: deep navy ground, frosted card surfaces, and a
/// violet-to-blue accent kept separate from the status colours.
/// </summary>
public static class UiTheme
{
    public static readonly Color Ink = Color.FromArgb(10, 13, 28);
    public static readonly Color Backdrop = Color.FromArgb(16, 21, 44);
    public static readonly Color Card = Color.FromArgb(24, 31, 60);
    public static readonly Color CardHover = Color.FromArgb(32, 41, 76);
    public static readonly Color Border = Color.FromArgb(46, 58, 100);
    public static readonly Color Text = Color.FromArgb(238, 242, 255);
    public static readonly Color TextSoft = Color.FromArgb(166, 180, 216);
    public static readonly Color Muted = Color.FromArgb(112, 126, 162);

    public static readonly Color Primary = Color.FromArgb(139, 92, 246);
    public static readonly Color Secondary = Color.FromArgb(59, 130, 246);
    public static readonly Color Tertiary = Color.FromArgb(34, 211, 238);
    public static readonly Color Success = Color.FromArgb(34, 197, 94);
    public static readonly Color Warning = Color.FromArgb(245, 158, 11);
    public static readonly Color Danger = Color.FromArgb(244, 63, 94);

    public static readonly Font Display = new("Segoe UI Semibold", 15F);
    public static readonly Font Heading = new("Segoe UI Semibold", 10.5F);
    public static readonly Font Body = new("Segoe UI", 9.75F);
    public static readonly Font Value = new("Segoe UI Semibold", 20F);
    public static readonly Font Caption = new("Segoe UI", 8.25F);

    /// <summary>Rounded rectangle path, used by every card and pill.</summary>
    public static GraphicsPath RoundedRect(Rectangle bounds, int radius)
    {
        var path = new GraphicsPath();
        var diameter = radius * 2;
        if (diameter <= 0 || bounds.Width <= diameter || bounds.Height <= diameter)
        {
            path.AddRectangle(bounds);
            return path;
        }
        path.AddArc(bounds.X, bounds.Y, diameter, diameter, 180, 90);
        path.AddArc(bounds.Right - diameter - 1, bounds.Y, diameter, diameter, 270, 90);
        path.AddArc(bounds.Right - diameter - 1, bounds.Bottom - diameter - 1, diameter, diameter, 0, 90);
        path.AddArc(bounds.X, bounds.Bottom - diameter - 1, diameter, diameter, 90, 90);
        path.CloseFigure();
        return path;
    }

    public static void FillCard(Graphics graphics, Rectangle bounds, Color? accent = null)
    {
        graphics.SmoothingMode = SmoothingMode.AntiAlias;
        using var path = RoundedRect(bounds, 10);
        using var body = new LinearGradientBrush(bounds, Card, Backdrop, 90F);
        graphics.FillPath(body, path);
        using var pen = new Pen(accent is null ? Border : Blend(Border, accent.Value, 0.55f));
        graphics.DrawPath(pen, path);
    }

    public static Color Blend(Color a, Color b, float amount)
    {
        amount = Math.Clamp(amount, 0f, 1f);
        return Color.FromArgb(
            (int)(a.R + (b.R - a.R) * amount),
            (int)(a.G + (b.G - a.G) * amount),
            (int)(a.B + (b.B - a.B) * amount));
    }

    /// <summary>Text input styled as a recessed well.</summary>
    public static TextBox Field(bool password = false)
    {
        var box = new TextBox
        {
            Dock = DockStyle.Fill,
            BorderStyle = BorderStyle.FixedSingle,
            BackColor = Ink,
            ForeColor = Text,
            Font = Body,
            UseSystemPasswordChar = password,
            Margin = new Padding(0, 3, 0, 3),
        };
        return box;
    }

    public static NumericUpDown Number(decimal minimum, decimal maximum, decimal value) => new()
    {
        Dock = DockStyle.Fill,
        Minimum = minimum,
        Maximum = maximum,
        Value = value,
        DecimalPlaces = 0,
        BorderStyle = BorderStyle.FixedSingle,
        BackColor = Ink,
        ForeColor = Text,
        Font = Body,
        Margin = new Padding(0, 3, 0, 3),
    };

    public static Label Caption2(string text) => new()
    {
        Text = text,
        AutoSize = true,
        ForeColor = Muted,
        Font = Caption,
        Margin = new Padding(0, 6, 0, 0),
    };

    /// <summary>Filled gradient button for the primary action of a page.</summary>
    public static Button Primary1(string text, EventHandler onClick) =>
        StyledButton(text, onClick, Primary, Secondary, Text);

    /// <summary>Quiet button for secondary actions.</summary>
    public static Button Ghost(string text, EventHandler onClick) =>
        StyledButton(text, onClick, Card, CardHover, TextSoft);

    /// <summary>Button for destructive actions, coloured by status not accent.</summary>
    public static Button Destructive(string text, EventHandler onClick) =>
        StyledButton(text, onClick, Color.FromArgb(72, 24, 40), Color.FromArgb(104, 32, 52), Color.FromArgb(255, 176, 190));

    private static Button StyledButton(string text, EventHandler onClick, Color from, Color to, Color foreground)
    {
        var button = new Button
        {
            Text = text,
            AutoSize = false,
            Height = 34,
            Width = 132,
            FlatStyle = FlatStyle.Flat,
            ForeColor = foreground,
            Font = Body,
            Margin = new Padding(0, 0, 8, 0),
            Cursor = Cursors.Hand,
        };
        button.FlatAppearance.BorderSize = 0;
        button.Paint += (sender, args) =>
        {
            if (sender is not Button target) return;
            var bounds = new Rectangle(0, 0, target.Width, target.Height);
            args.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            using var path = RoundedRect(bounds, 8);
            var hovered = target.ClientRectangle.Contains(target.PointToClient(Cursor.Position));
            using var brush = new LinearGradientBrush(bounds,
                hovered ? Blend(from, Color.White, 0.10f) : from,
                hovered ? Blend(to, Color.White, 0.10f) : to, 0F);
            args.Graphics.FillPath(brush, path);
            TextRenderer.DrawText(args.Graphics, target.Text, target.Font, bounds,
                target.ForeColor, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
        };
        button.MouseEnter += (sender, _) => ((Control)sender!).Invalidate();
        button.MouseLeave += (sender, _) => ((Control)sender!).Invalidate();
        button.Click += onClick;
        return button;
    }
}

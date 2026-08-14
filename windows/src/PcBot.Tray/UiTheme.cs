using System.Drawing.Drawing2D;

namespace PcBot.Tray;

/// <summary>
/// Dark theme shared by every control, matching the palette the device runs:
/// deep navy ground, frosted card surfaces, and a violet-to-blue accent kept
/// separate from the status colours.
///
/// Nothing here uses fixed pixel heights for text: sizes come from the fonts so
/// the window lays out correctly at 100%, 125% and 150% display scaling.
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

    public static readonly Color Accent = Color.FromArgb(139, 92, 246);
    public static readonly Color AccentEnd = Color.FromArgb(59, 130, 246);
    public static readonly Color Highlight = Color.FromArgb(34, 211, 238);
    public static readonly Color Success = Color.FromArgb(34, 197, 94);
    public static readonly Color Warning = Color.FromArgb(245, 158, 11);
    public static readonly Color Danger = Color.FromArgb(244, 63, 94);

    public static readonly Font Brand = new("Segoe UI Semibold", 16F);
    public static readonly Font BrandSub = new("Segoe UI", 8F);
    public static readonly Font Display = new("Segoe UI Semibold", 14F);
    public static readonly Font Heading = new("Segoe UI Semibold", 9.5F);
    public static readonly Font Body = new("Segoe UI", 9.75F);
    public static readonly Font Value = new("Segoe UI Semibold", 19F);
    public static readonly Font Small = new("Segoe UI", 8.25F);

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
        path.AddArc(bounds.Right - diameter, bounds.Y, diameter, diameter, 270, 90);
        path.AddArc(bounds.Right - diameter, bounds.Bottom - diameter, diameter, diameter, 0, 90);
        path.AddArc(bounds.X, bounds.Bottom - diameter, diameter, diameter, 90, 90);
        path.CloseFigure();
        return path;
    }

    public static Color Blend(Color a, Color b, float amount)
    {
        amount = Math.Clamp(amount, 0f, 1f);
        return Color.FromArgb(
            (int)(a.R + (b.R - a.R) * amount),
            (int)(a.G + (b.G - a.G) * amount),
            (int)(a.B + (b.B - a.B) * amount));
    }

    /// <summary>Label that sits on a card, vertically centred against its row.</summary>
    public static Label FieldLabel(string text) => new()
    {
        Text = text,
        AutoSize = true,
        ForeColor = TextSoft,
        Font = Body,
        Anchor = AnchorStyles.Left,
        Margin = new Padding(0, 9, 12, 9),
    };

    public static Label Caption(string text) => new()
    {
        Text = text,
        AutoSize = true,
        ForeColor = Muted,
        Font = Small,
        Margin = new Padding(0, 2, 0, 0),
    };

    public static TextBox Field(bool password = false) => new()
    {
        Dock = DockStyle.Fill,
        BorderStyle = BorderStyle.FixedSingle,
        BackColor = Ink,
        ForeColor = Text,
        Font = Body,
        UseSystemPasswordChar = password,
        Margin = new Padding(0, 5, 0, 5),
    };

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
        Margin = new Padding(0, 5, 0, 5),
    };

    public static Button PrimaryButton(string text, EventHandler onClick) =>
        StyledButton(text, onClick, Accent, AccentEnd, Text);

    public static Button GhostButton(string text, EventHandler onClick) =>
        StyledButton(text, onClick, Card, CardHover, TextSoft);

    public static Button DestructiveButton(string text, EventHandler onClick) =>
        StyledButton(text, onClick, Color.FromArgb(74, 26, 42), Color.FromArgb(108, 34, 56),
            Color.FromArgb(255, 176, 190));

    private static Button StyledButton(string text, EventHandler onClick, Color from, Color to,
        Color foreground)
    {
        var button = new Button
        {
            Text = text,
            AutoSize = false,
            FlatStyle = FlatStyle.Flat,
            ForeColor = foreground,
            // Matching the surface behind it keeps the rounded corners from
            // showing a square patch of the default control colour.
            BackColor = Card,
            Font = Body,
            Margin = new Padding(0, 0, 10, 0),
            Cursor = Cursors.Hand,
            UseVisualStyleBackColor = false,
        };
        button.FlatAppearance.BorderSize = 0;
        button.FlatAppearance.MouseOverBackColor = button.BackColor;
        button.FlatAppearance.MouseDownBackColor = button.BackColor;

        // Size from the text so long labels are never clipped at any DPI.
        var measured = TextRenderer.MeasureText(text, Body);
        button.Size = new Size(measured.Width + 34, measured.Height + 18);

        var hovered = false;
        button.Paint += (sender, args) =>
        {
            if (sender is not Button target) return;
            var bounds = new Rectangle(0, 0, target.Width, target.Height);
            args.Graphics.Clear(target.BackColor);
            args.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            using var path = RoundedRect(new Rectangle(0, 0, bounds.Width - 1, bounds.Height - 1), 8);
            using var brush = new LinearGradientBrush(bounds,
                hovered ? Blend(from, Color.White, 0.12f) : from,
                hovered ? Blend(to, Color.White, 0.12f) : to, 0F);
            args.Graphics.FillPath(brush, path);
            TextRenderer.DrawText(args.Graphics, target.Text, target.Font, bounds,
                target.ForeColor, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
        };
        button.MouseEnter += (sender, _) => { hovered = true; ((Control)sender!).Invalidate(); };
        button.MouseLeave += (sender, _) => { hovered = false; ((Control)sender!).Invalidate(); };
        button.Click += onClick;
        return button;
    }
}

using System.Text.Json;

namespace PcBot.Core;

public static class OptionsStore
{
    public static string ConfigDirectory => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "PcBot");
    public static string ConfigPath => Path.Combine(ConfigDirectory, "agent.json");

    public static AgentOptions Load()
    {
        if (!File.Exists(ConfigPath)) return new AgentOptions();
        var json = File.ReadAllText(ConfigPath);
        return JsonSerializer.Deserialize<AgentOptions>(json, JsonOptions()) ?? new AgentOptions();
    }

    public static void Save(AgentOptions options)
    {
        Directory.CreateDirectory(ConfigDirectory);
        var temporary = ConfigPath + ".tmp";
        File.WriteAllText(temporary, JsonSerializer.Serialize(options, JsonOptions()));
        File.Move(temporary, ConfigPath, true);
    }

    private static JsonSerializerOptions JsonOptions() => new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
        PropertyNameCaseInsensitive = true,
    };
}


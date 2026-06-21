using System.Diagnostics;
using System.Text.Json;
using PcBot.Core;

namespace PcBot.Service;

public sealed class SpeedTestRunner
{
    public async Task<SpeedTestState> RunAsync(string executable, CancellationToken cancellationToken)
    {
        try {
            var start = new ProcessStartInfo(executable,
                "--accept-license --accept-gdpr --format=json")
            {
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true,
            };
            using var process = Process.Start(start) ?? throw new InvalidOperationException("Could not start Speedtest CLI");
            var outputTask = process.StandardOutput.ReadToEndAsync(cancellationToken);
            var errorTask = process.StandardError.ReadToEndAsync(cancellationToken);
            await process.WaitForExitAsync(cancellationToken);
            var output = await outputTask;
            var error = await errorTask;
            if (process.ExitCode != 0) throw new InvalidOperationException(error.Trim());
            using var doc = JsonDocument.Parse(output);
            var root = doc.RootElement;
            return new SpeedTestState("complete",
                root.GetProperty("download").GetProperty("bandwidth").GetDouble() * 8 / 1_000_000,
                root.GetProperty("upload").GetProperty("bandwidth").GetDouble() * 8 / 1_000_000,
                root.GetProperty("ping").GetProperty("latency").GetDouble(),
                root.GetProperty("server").GetProperty("name").GetString(), null, DateTimeOffset.UtcNow);
        } catch (OperationCanceledException) {
            return new SpeedTestState("cancelled", null, null, null, null, "Cancelled", DateTimeOffset.UtcNow);
        } catch (Exception ex) {
            return new SpeedTestState("failed", null, null, null, null, ex.Message, DateTimeOffset.UtcNow);
        }
    }
}

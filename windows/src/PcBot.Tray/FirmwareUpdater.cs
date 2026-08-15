using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text.Json;

namespace PcBot.Tray;

/// <summary>
/// Pushes a firmware.bin to the device's LAN update endpoint. The device writes
/// it to its inactive OTA slot and only swaps over once the image verifies, so
/// an interrupted upload leaves the running firmware alone.
/// </summary>
public sealed class FirmwareUpdater
{
    private const string TokenHeader = "X-Pico-Token";
    private static readonly TimeSpan Timeout = TimeSpan.FromMinutes(4);

    /// <summary>Device details reported by GET /info.</summary>
    public sealed record DeviceInfo(string DeviceId, string Version, string Ip,
        long SketchSize, long FreeSpace, bool Authorised);

    public async Task<DeviceInfo> QueryAsync(string ip, string token, CancellationToken cancellation)
    {
        using var client = new HttpClient { Timeout = TimeSpan.FromSeconds(8) };
        client.DefaultRequestHeaders.Add(TokenHeader, token);
        var payload = await client.GetStringAsync($"http://{ip}/info", cancellation);
        using var document = JsonDocument.Parse(payload);
        var root = document.RootElement;
        return new DeviceInfo(
            root.GetProperty("deviceId").GetString() ?? "",
            root.GetProperty("version").GetString() ?? "",
            root.GetProperty("ip").GetString() ?? "",
            root.GetProperty("sketchSize").GetInt64(),
            root.GetProperty("freeSpace").GetInt64(),
            root.GetProperty("authorised").GetBoolean());
    }

    /// <summary>
    /// Uploads the image, reporting 0-100 as it goes. Returns the device's
    /// response message; throws if the transfer or the device rejects it.
    /// </summary>
    public async Task<string> UploadAsync(string ip, string token, string firmwarePath,
        IProgress<int> progress, CancellationToken cancellation)
    {
        var file = new FileInfo(firmwarePath);
        if (!file.Exists) throw new FileNotFoundException("Firmware image not found.", firmwarePath);
        if (file.Length < 64 * 1024)
            throw new InvalidOperationException("That file is too small to be an ESP32 image.");

        using var client = new HttpClient { Timeout = Timeout };
        client.DefaultRequestHeaders.Add(TokenHeader, token);

        await using var stream = file.OpenRead();
        using var content = new MultipartFormDataContent();
        var body = new ProgressStreamContent(stream, file.Length, progress);
        body.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
        content.Add(body, "firmware", file.Name);

        using var response = await client.PostAsync($"http://{ip}/update", content, cancellation);
        var text = await response.Content.ReadAsStringAsync(cancellation);
        if (response.StatusCode == System.Net.HttpStatusCode.Unauthorized)
            throw new InvalidOperationException("Device rejected the update token.");
        if (!response.IsSuccessStatusCode)
            throw new InvalidOperationException($"Device refused the image: {text}");
        return text;
    }

    /// <summary>Streams a file while reporting progress, so a 1.2 MB push is not a frozen dialog.</summary>
    private sealed class ProgressStreamContent(Stream stream, long length, IProgress<int> progress)
        : HttpContent
    {
        private const int BufferSize = 16 * 1024;

        protected override async Task SerializeToStreamAsync(Stream target, TransportContext? context)
        {
            var buffer = new byte[BufferSize];
            long sent = 0;
            int read;
            while ((read = await stream.ReadAsync(buffer)) > 0)
            {
                await target.WriteAsync(buffer.AsMemory(0, read));
                sent += read;
                progress.Report((int)(sent * 100 / Math.Max(length, 1)));
            }
        }

        protected override bool TryComputeLength(out long computedLength)
        {
            computedLength = length;
            return true;
        }
    }
}

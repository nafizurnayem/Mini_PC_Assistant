using System.Runtime.InteropServices;

namespace PcBot.Tray;

/// <summary>
/// Master volume of the default playback device, through Core Audio.
/// Hand-written COM interop rather than a package: the surface needed here is
/// three interfaces, and the agent must keep working on a machine that cannot
/// restore NuGet packages.
/// </summary>
public sealed class SystemVolume : IDisposable
{
    private const int RenderDataFlow = 0;
    private const int ConsoleRole = 0;
    private const int ClsCtxAll = 23;

    private readonly IAudioEndpointVolume _endpoint;
    private bool _disposed;

    public SystemVolume()
    {
        var enumeratorType = Type.GetTypeFromCLSID(new Guid("BCDE0395-E52F-467C-8E3D-C4579291692E"))
            ?? throw new InvalidOperationException("Core Audio device enumerator is unavailable.");
        var enumerator = (IMMDeviceEnumerator)Activator.CreateInstance(enumeratorType)!;
        enumerator.GetDefaultAudioEndpoint(RenderDataFlow, ConsoleRole, out var device);
        var volumeId = typeof(IAudioEndpointVolume).GUID;
        device.Activate(ref volumeId, ClsCtxAll, IntPtr.Zero, out var activated);
        _endpoint = (IAudioEndpointVolume)activated;
    }

    /// <summary>Current master level as a percentage, 0-100.</summary>
    public int GetLevel()
    {
        _endpoint.GetMasterVolumeLevelScalar(out var scalar);
        return (int)Math.Round(Math.Clamp(scalar, 0f, 1f) * 100f);
    }

    public bool GetMuted()
    {
        _endpoint.GetMute(out var muted);
        return muted;
    }

    public void SetLevel(int percent)
    {
        var scalar = Math.Clamp(percent, 0, 100) / 100f;
        _endpoint.SetMasterVolumeLevelScalar(scalar, Guid.Empty);
        // A knob at zero means silence, so follow through on mute state too.
        _endpoint.SetMute(percent <= 0, Guid.Empty);
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        if (Marshal.IsComObject(_endpoint)) Marshal.ReleaseComObject(_endpoint);
    }

    [ComImport, Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IMMDeviceEnumerator
    {
        int NotImpl1();
        int GetDefaultAudioEndpoint(int dataFlow, int role, out IMMDevice device);
    }

    [ComImport, Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IMMDevice
    {
        int Activate(ref Guid interfaceId, int classContext, IntPtr activationParams,
            [MarshalAs(UnmanagedType.IUnknown)] out object instance);
    }

    [ComImport, Guid("5CDF2C82-841E-4546-9722-0CF74078229A"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IAudioEndpointVolume
    {
        int RegisterControlChangeNotify(IntPtr client);
        int UnregisterControlChangeNotify(IntPtr client);
        int GetChannelCount(out int count);
        int SetMasterVolumeLevel(float levelDb, Guid eventContext);
        int SetMasterVolumeLevelScalar(float level, Guid eventContext);
        int GetMasterVolumeLevel(out float levelDb);
        int GetMasterVolumeLevelScalar(out float level);
        int SetChannelVolumeLevel(uint channel, float levelDb, Guid eventContext);
        int SetChannelVolumeLevelScalar(uint channel, float level, Guid eventContext);
        int GetChannelVolumeLevel(uint channel, out float levelDb);
        int GetChannelVolumeLevelScalar(uint channel, out float level);
        int SetMute([MarshalAs(UnmanagedType.Bool)] bool mute, Guid eventContext);
        int GetMute([MarshalAs(UnmanagedType.Bool)] out bool mute);
    }
}

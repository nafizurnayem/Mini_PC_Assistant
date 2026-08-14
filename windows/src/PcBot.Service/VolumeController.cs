using System.Runtime.InteropServices;

namespace PcBot.Service;

/// <summary>
/// Sets the Windows default playback device master volume (0–100)
/// using the Core Audio COM API. No external NuGet packages required.
/// </summary>
internal static class VolumeController
{
    private static readonly Guid AevGuid = new("5CDF2C82-841E-4546-9722-0CF74078229A");

    /// <summary>Sets the master volume. Level is clamped to [0, 100].</summary>
    public static void SetLevel(int level)
    {
        level = Math.Clamp(level, 0, 100);

        var enumerator = (IMMDeviceEnumerator)new MmDeviceEnumeratorClass();
        try
        {
            enumerator.GetDefaultAudioEndpoint(EDataFlow.eRender, ERole.eMultimedia, out var device);
            try
            {
                var iid = AevGuid;
                device.Activate(ref iid, 1 /* CLSCTX_INPROC_SERVER */, IntPtr.Zero, out var volObj);
                var vol = (IAudioEndpointVolume)volObj;
                try
                {
                    var ctx = Guid.Empty;
                    vol.SetMasterVolumeLevelScalar(level / 100.0f, ref ctx);
                }
                finally { Marshal.ReleaseComObject(vol); }
            }
            finally { Marshal.ReleaseComObject(device); }
        }
        finally { Marshal.ReleaseComObject(enumerator); }
    }
}

// ── COM enums ────────────────────────────────────────────────────────────────

internal enum EDataFlow { eRender, eCapture, eAll }
internal enum ERole    { eConsole, eMultimedia, eCommunications }

// ── MMDeviceEnumerator COM class ─────────────────────────────────────────────

[ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
internal class MmDeviceEnumeratorClass { }

// ── Interfaces (vtable order must match Windows SDK exactly) ─────────────────

[ComImport, Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
internal interface IMMDeviceEnumerator
{
    void EnumAudioEndpoints(EDataFlow dataFlow, int stateMask, out IntPtr ppDevices);
    void GetDefaultAudioEndpoint(EDataFlow dataFlow, ERole role, out IMMDevice ppEndpoint);
    void GetDevice([MarshalAs(UnmanagedType.LPWStr)] string id, out IMMDevice ppDevice);
    void RegisterEndpointNotificationCallback(IntPtr pClient);
    void UnregisterEndpointNotificationCallback(IntPtr pClient);
}

[ComImport, Guid("D666063F-1587-4E43-81F1-B948E807363F"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
internal interface IMMDevice
{
    void Activate(ref Guid iid, int clsCtx, IntPtr pActivationParams,
                  [MarshalAs(UnmanagedType.IUnknown)] out object ppInterface);
    void OpenPropertyStore(int stgmAccess, out IntPtr ppProperties);
    void GetId([MarshalAs(UnmanagedType.LPWStr)] out string id);
    void GetState(out int state);
}

[ComImport, Guid("5CDF2C82-841E-4546-9722-0CF74078229A"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
internal interface IAudioEndpointVolume
{
    void RegisterControlChangeNotify(IntPtr pNotify);
    void UnregisterControlChangeNotify(IntPtr pNotify);
    void GetChannelCount(out uint pnChannelCount);
    void SetMasterVolumeLevel(float fLevelDB, ref Guid pguidEventContext);
    void SetMasterVolumeLevelScalar(float fLevel, ref Guid pguidEventContext);
    void GetMasterVolumeLevel(out float pfLevelDB);
    void GetMasterVolumeLevelScalar(out float pfLevel);
    void SetChannelVolumeLevel(uint nChannel, float fLevelDB, ref Guid pguidEventContext);
    void SetChannelVolumeLevelScalar(uint nChannel, float fLevel, ref Guid pguidEventContext);
    void GetChannelVolumeLevel(uint nChannel, out float pfLevelDB);
    void GetChannelVolumeLevelScalar(uint nChannel, out float pfLevel);
    void SetMute([MarshalAs(UnmanagedType.Bool)] bool bMute, ref Guid pguidEventContext);
    void GetMute([MarshalAs(UnmanagedType.Bool)] out bool pbMute);
    void GetVolumeStepInfo(out uint pnStep, out uint pnStepCount);
    void VolumeStepUp(ref Guid pguidEventContext);
    void VolumeStepDown(ref Guid pguidEventContext);
    void QueryHardwareSupport(out uint pdwHardwareSupportMask);
    void GetVolumeRange(out float pflVolumeMindB, out float pflVolumeMaxdB,
                        out float pflVolumeIncrementdB);
}

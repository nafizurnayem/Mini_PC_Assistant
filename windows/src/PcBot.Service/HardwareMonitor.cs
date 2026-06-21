using System.Net.NetworkInformation;
using System.Runtime.InteropServices;
using LibreHardwareMonitor.Hardware;
using PcBot.Core;

namespace PcBot.Service;

public sealed class HardwareMonitor : IDisposable
{
    private readonly Computer _computer = new()
    {
        IsCpuEnabled = true,
        IsGpuEnabled = true,
        IsMemoryEnabled = true,
        IsStorageEnabled = true,
        IsNetworkEnabled = true,
    };
    private long _lastReceived;
    private long _lastSent;
    private DateTimeOffset _lastNetworkSample = DateTimeOffset.UtcNow;

    public HardwareMonitor() => _computer.Open();

    public TelemetrySnapshot Read()
    {
        _computer.Accept(new UpdateVisitor());
        var cpu = ReadMetric(HardwareType.Cpu);
        var gpu = ReadMetric(HardwareType.GpuAmd, HardwareType.GpuIntel, HardwareType.GpuNvidia);
        var memory = ReadMemory();
        var storage = ReadStorage();
        var network = ReadNetwork();
        return new TelemetrySnapshot(
            DateTimeOffset.UtcNow, cpu, gpu, memory, storage, network,
            Environment.TickCount64 / 1000);
    }

    private Metric ReadMetric(params HardwareType[] types)
    {
        var hardware = AllHardware().Where(h => types.Contains(h.HardwareType)).ToArray();
        var temperatures = hardware.SelectMany(h => h.Sensors)
            .Where(s => s.SensorType == SensorType.Temperature && s.Value.HasValue)
            .Select(s => (double)s.Value!.Value).ToArray();
        var loads = hardware.SelectMany(h => h.Sensors)
            .Where(s => s.SensorType == SensorType.Load && s.Value.HasValue &&
                        (s.Name.Contains("Total", StringComparison.OrdinalIgnoreCase) || hardware.Length == 1))
            .Select(s => (double)s.Value!.Value).ToArray();
        return new Metric(temperatures.Length == 0 ? null : temperatures.Max(),
            loads.Length == 0 ? null : loads.Max());
    }

    private static MemoryMetric ReadMemory()
    {
        var status = new MemoryStatusEx();
        if (!GlobalMemoryStatusEx(status)) throw new InvalidOperationException("GlobalMemoryStatusEx failed");
        var used = status.TotalPhys - status.AvailPhys;
        return new MemoryMetric(status.MemoryLoad, used, status.TotalPhys);
    }

    private IReadOnlyList<StorageMetric> ReadStorage()
    {
        var temperatures = AllHardware()
            .Where(h => h.HardwareType == HardwareType.Storage)
            .SelectMany(h => h.Sensors.Where(s => s.SensorType == SensorType.Temperature && s.Value.HasValue)
                .Select(s => (Name: h.Name, Value: (double?)s.Value!.Value)))
            .GroupBy(x => x.Name).ToDictionary(g => g.Key, g => g.Max(x => x.Value));
        return DriveInfo.GetDrives().Where(d => d.IsReady && d.DriveType == DriveType.Fixed)
            .Select(d => {
                var total = d.TotalSize;
                var usedPercent = total == 0 ? 0 : (total - d.AvailableFreeSpace) * 100d / total;
                var temperature = temperatures.FirstOrDefault(x =>
                    x.Key.Contains(d.VolumeLabel, StringComparison.OrdinalIgnoreCase)).Value;
                return new StorageMetric(d.Name, temperature, usedPercent, d.AvailableFreeSpace, total);
            }).ToArray();
    }

    private NetworkMetric ReadNetwork()
    {
        var active = NetworkInterface.GetAllNetworkInterfaces()
            .Where(n => n.OperationalStatus == OperationalStatus.Up &&
                        n.NetworkInterfaceType is not NetworkInterfaceType.Loopback and not NetworkInterfaceType.Tunnel)
            .ToArray();
        var received = active.Sum(n => n.GetIPv4Statistics().BytesReceived);
        var sent = active.Sum(n => n.GetIPv4Statistics().BytesSent);
        var now = DateTimeOffset.UtcNow;
        var seconds = Math.Max(0.001, (now - _lastNetworkSample).TotalSeconds);
        var download = _lastReceived == 0 ? 0 : (received - _lastReceived) * 8d / seconds / 1_000_000;
        var upload = _lastSent == 0 ? 0 : (sent - _lastSent) * 8d / seconds / 1_000_000;
        _lastReceived = received;
        _lastSent = sent;
        _lastNetworkSample = now;
        return new NetworkMetric(Math.Max(0, download), Math.Max(0, upload),
            string.Join(", ", active.Select(n => n.Name)));
    }

    private IEnumerable<IHardware> AllHardware()
    {
        foreach (var hardware in _computer.Hardware) {
            yield return hardware;
            foreach (var child in Descendants(hardware)) yield return child;
        }
    }

    private static IEnumerable<IHardware> Descendants(IHardware hardware)
    {
        foreach (var child in hardware.SubHardware) {
            yield return child;
            foreach (var nested in Descendants(child)) yield return nested;
        }
    }

    public void Dispose() => _computer.Close();

    private sealed class UpdateVisitor : IVisitor
    {
        public void VisitComputer(IComputer computer) => computer.Traverse(this);
        public void VisitHardware(IHardware hardware) { hardware.Update(); foreach (var sub in hardware.SubHardware) sub.Accept(this); }
        public void VisitSensor(ISensor sensor) { }
        public void VisitParameter(IParameter parameter) { }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
    private sealed class MemoryStatusEx
    {
        public uint Length = (uint)Marshal.SizeOf<MemoryStatusEx>();
        public uint MemoryLoad;
        public ulong TotalPhys;
        public ulong AvailPhys;
        public ulong TotalPageFile;
        public ulong AvailPageFile;
        public ulong TotalVirtual;
        public ulong AvailVirtual;
        public ulong AvailExtendedVirtual;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GlobalMemoryStatusEx([In, Out] MemoryStatusEx buffer);
}


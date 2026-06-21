import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'app_controller.dart';
import 'ble_provisioning.dart';
import 'models.dart';
import 'mqtt_service.dart';
import 'secure_settings.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(ChangeNotifierProvider(
    create: (_) => AppController(SecureSettings(), MqttService())..initialize(),
    child: const PcBotApp(),
  ));
}

class PcBotApp extends StatelessWidget {
  const PcBotApp({super.key});

  @override
  Widget build(BuildContext context) => MaterialApp(
        title: 'PC Bot',
        theme: ThemeData(
          colorScheme: ColorScheme.fromSeed(seedColor: const Color(0xff6750a4), brightness: Brightness.dark),
          useMaterial3: true,
        ),
        home: const HomeScreen(),
      );
}

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});
  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  var index = 0;

  @override
  Widget build(BuildContext context) {
    final pages = [const Dashboard(), const HistoryScreen(), const SettingsScreen()];
    return Scaffold(
      appBar: AppBar(title: const Text('PC Assistant Bot')),
      body: pages[index],
      bottomNavigationBar: NavigationBar(
        selectedIndex: index,
        onDestinationSelected: (value) => setState(() => index = value),
        destinations: const [
          NavigationDestination(icon: Icon(Icons.monitor_heart_outlined), label: 'Dashboard'),
          NavigationDestination(icon: Icon(Icons.history), label: 'History'),
          NavigationDestination(icon: Icon(Icons.settings), label: 'Settings'),
        ],
      ),
    );
  }
}

class Dashboard extends StatelessWidget {
  const Dashboard({super.key});

  @override
  Widget build(BuildContext context) {
    final state = context.watch<AppController>();
    final telemetry = state.telemetry;
    return RefreshIndicator(
      onRefresh: state.reconnect,
      child: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Row(children: [
            StatusChip('Cloud', state.connected),
            const SizedBox(width: 8),
            StatusChip('Bot', state.botOnline),
            const SizedBox(width: 8),
            StatusChip('PC', state.pcOnline),
          ]),
          if (state.error != null) Padding(
            padding: const EdgeInsets.only(top: 12),
            child: Text(state.error!, style: TextStyle(color: Theme.of(context).colorScheme.error)),
          ),
          const SizedBox(height: 20),
          FilledButton.icon(
            onPressed: state.connected ? () => _confirmPower(context, state) : null,
            icon: const Icon(Icons.power_settings_new),
            label: Text(state.pcOnline ? 'Shut down PC' : 'Wake PC'),
          ),
          const SizedBox(height: 12),
          OutlinedButton.icon(
            onPressed: state.pcOnline ? state.runSpeedTest : null,
            icon: const Icon(Icons.speed),
            label: const Text('Run internet speed test'),
          ),
          const SizedBox(height: 20),
          Wrap(spacing: 12, runSpacing: 12, children: [
            MetricCard('CPU', telemetry?.cpuTemp, '°C', telemetry?.cpuLoad, '% load'),
            MetricCard('GPU', telemetry?.gpuTemp, '°C', telemetry?.gpuLoad, '% load'),
            MetricCard('RAM', telemetry?.memoryUsed, '%', null, ''),
            MetricCard('Download', telemetry?.downloadMbps, 'Mbps', telemetry?.uploadMbps, 'Mbps up'),
          ]),
          if (state.speedResult != null) Card(
            child: ListTile(
              leading: const Icon(Icons.network_check),
              title: Text('Speed test: ${state.speedResult!.status}'),
              subtitle: Text(state.speedResult!.error ??
                  '${fmt(state.speedResult!.download)} Mbps down · ${fmt(state.speedResult!.upload)} Mbps up · ${fmt(state.speedResult!.ping)} ms'),
            ),
          ),
          for (final drive in telemetry?.storage ?? const <Map<String, dynamic>>[])
            Card(child: ListTile(
              leading: const Icon(Icons.storage),
              title: Text(drive['name']?.toString() ?? 'Drive'),
              subtitle: Text('${fmt(drive['usedPercent'] as num?)}% used · ${fmt(drive['temperatureC'] as num?)}°C'),
            )),
        ],
      ),
    );
  }

  Future<void> _confirmPower(BuildContext context, AppController state) async {
    final off = state.pcOnline;
    final confirmed = await showDialog<bool>(context: context, builder: (context) => AlertDialog(
      title: Text(off ? 'Shut down PC?' : 'Wake PC?'),
      content: Text(off ? 'Windows will close through a graceful shutdown command.' : 'ESP32-S3 will send Wake-on-LAN locally.'),
      actions: [
        TextButton(onPressed: () => Navigator.pop(context, false), child: const Text('Cancel')),
        FilledButton(onPressed: () => Navigator.pop(context, true), child: Text(off ? 'Shut down' : 'Wake')),
      ],
    ));
    if (confirmed == true) state.power(!off);
  }
}

class HistoryScreen extends StatelessWidget {
  const HistoryScreen({super.key});
  @override
  Widget build(BuildContext context) {
    final state = context.watch<AppController>();
    if (state.history.isEmpty && state.alerts.isEmpty) return const Center(child: Text('No telemetry yet'));
    return ListView(padding: const EdgeInsets.all(16), children: [
      Text('Alerts', style: Theme.of(context).textTheme.titleLarge),
      for (final alert in state.alerts) ListTile(
        leading: Icon(alert['severity'] == 'critical' ? Icons.error : Icons.warning_amber),
        title: Text(alert['message']?.toString() ?? 'Alert'),
        subtitle: Text(alert['timestamp']?.toString() ?? ''),
      ),
      Text('Recent telemetry', style: Theme.of(context).textTheme.titleLarge),
      for (final item in state.history) ListTile(
        title: Text('CPU ${fmt(item.cpuTemp)}°C · GPU ${fmt(item.gpuTemp)}°C · RAM ${fmt(item.memoryUsed)}%'),
        subtitle: Text(item.timestamp.toLocal().toString()),
      ),
    ]);
  }
}

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});
  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  final fields = <String, TextEditingController>{};
  bool initialized = false;

  @override
  Widget build(BuildContext context) {
    final controller = context.watch<AppController>();
    if (!initialized) {
      final s = controller.settings;
      fields.addAll({
        'device': TextEditingController(text: s.deviceId),
        'host': TextEditingController(text: s.host),
        'port': TextEditingController(text: '${s.port}'),
        'user': TextEditingController(text: s.username),
        'password': TextEditingController(text: s.password),
        'mac': TextEditingController(text: s.pcMac),
        'broadcast': TextEditingController(text: s.broadcastIp),
        'ca': TextEditingController(text: s.rootCa),
        'ssid': TextEditingController(),
        'wifiPassword': TextEditingController(),
      });
      initialized = true;
    }
    return ListView(padding: const EdgeInsets.all(16), children: [
      input('Device ID', 'device'),
      input('HiveMQ host', 'host'),
      input('TLS port', 'port', number: true),
      input('MQTT username', 'user'),
      input('MQTT password', 'password', secret: true),
      input('PC MAC address', 'mac'),
      input('LAN broadcast address', 'broadcast'),
      input('Broker root CA PEM', 'ca', lines: 4),
      FilledButton(onPressed: () => _save(controller), child: const Text('Save and reconnect')),
      const Divider(height: 36),
      Text('Bot Wi-Fi provisioning', style: Theme.of(context).textTheme.titleLarge),
      input('Wi-Fi SSID', 'ssid'),
      input('Wi-Fi password', 'wifiPassword', secret: true),
      OutlinedButton(onPressed: () => _provision(controller), child: const Text('Find and provision bot')),
      const Divider(height: 36),
      OutlinedButton(onPressed: controller.connected ? controller.resetWifi : null, child: const Text('Reset bot Wi-Fi')),
      TextButton(onPressed: controller.connected ? () => _factoryReset(controller) : null,
          child: const Text('Factory reset bot')),
    ]);
  }

  Widget input(String label, String key, {bool secret = false, bool number = false, int lines = 1}) => Padding(
    padding: const EdgeInsets.only(bottom: 12),
    child: TextField(
      controller: fields[key],
      obscureText: secret,
      maxLines: lines,
      keyboardType: number ? TextInputType.number : TextInputType.text,
      decoration: InputDecoration(labelText: label, border: const OutlineInputBorder()),
    ),
  );

  AppSettings read() => AppSettings(
    deviceId: fields['device']!.text.trim(),
    host: fields['host']!.text.trim(),
    port: int.tryParse(fields['port']!.text) ?? 8883,
    username: fields['user']!.text,
    password: fields['password']!.text,
    pcMac: fields['mac']!.text.trim(),
    broadcastIp: fields['broadcast']!.text.trim(),
    rootCa: fields['ca']!.text,
  );

  Future<void> _save(AppController controller) => controller.saveSettings(read());

  Future<void> _provision(AppController controller) async {
    await _save(controller);
    if (!mounted) return;
    showDialog(context: context, barrierDismissible: false,
        builder: (_) => const AlertDialog(content: Row(children: [CircularProgressIndicator(), SizedBox(width: 16), Text('Scanning...')])));
    try {
      final ble = BleProvisioning();
      final devices = await ble.scan();
      if (!mounted) return;
      Navigator.pop(context);
      if (devices.isEmpty) throw StateError('No PC Bot found. Reset Wi-Fi or factory-reset bot first.');
      final result = await ble.provision(devices.first.device, read(), fields['ssid']!.text, fields['wifiPassword']!.text);
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Bot: $result')));
    } catch (error) {
      if (!mounted) return;
      if (Navigator.canPop(context)) Navigator.pop(context);
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('$error')));
    }
  }

  Future<void> _factoryReset(AppController controller) async {
    final confirmed = await showDialog<bool>(context: context, builder: (context) => AlertDialog(
      title: const Text('Factory reset bot?'),
      content: const Text('Erases Wi-Fi, MQTT credentials, pairing, thresholds, and settings.'),
      actions: [
        TextButton(onPressed: () => Navigator.pop(context, false), child: const Text('Cancel')),
        FilledButton(onPressed: () => Navigator.pop(context, true), child: const Text('Erase')),
      ],
    ));
    if (confirmed == true) controller.factoryReset();
  }

  @override
  void dispose() {
    for (final controller in fields.values) { controller.dispose(); }
    super.dispose();
  }
}

class StatusChip extends StatelessWidget {
  const StatusChip(this.label, this.online, {super.key});
  final String label;
  final bool online;
  @override
  Widget build(BuildContext context) => Chip(
    avatar: Icon(Icons.circle, size: 12, color: online ? Colors.greenAccent : Colors.grey),
    label: Text(label),
  );
}

class MetricCard extends StatelessWidget {
  const MetricCard(this.label, this.value, this.unit, this.secondary, this.secondaryUnit, {super.key});
  final String label;
  final num? value;
  final String unit;
  final num? secondary;
  final String secondaryUnit;
  @override
  Widget build(BuildContext context) => SizedBox(width: 165, child: Card(child: Padding(
    padding: const EdgeInsets.all(16),
    child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      Text(label, style: Theme.of(context).textTheme.labelLarge),
      Text('${fmt(value)} $unit', style: Theme.of(context).textTheme.headlineSmall),
      if (secondary != null) Text('${fmt(secondary)} $secondaryUnit'),
    ]),
  )));
}

String fmt(num? value) => value == null ? '—' : value.toStringAsFixed(1);


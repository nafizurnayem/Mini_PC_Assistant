import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'ble_provisioning.dart';

class BotWifiSetupScreen extends StatefulWidget {
  const BotWifiSetupScreen({super.key});

  @override
  State<BotWifiSetupScreen> createState() => _BotWifiSetupScreenState();
}

class _BotWifiSetupScreenState extends State<BotWifiSetupScreen> {
  static const platform = MethodChannel('pcbot/setup');
  final ssid = TextEditingController();
  final password = TextEditingController();
  bool busy = false;
  String status = '';

  Future<void> open(String method) async {
    try {
      await platform.invokeMethod<void>(method);
    } on PlatformException catch (error) {
      if (mounted) {
        setState(() => status =
            error.message ?? 'Open Wi-Fi settings or the setup URL manually.');
      }
    } on MissingPluginException {
      if (mounted) {
        setState(() => status =
            'Open system Wi-Fi settings, join PICO-xxxxxx, then open http://192.168.4.1 in your browser.');
      }
    }
  }

  Future<void> sendWifi() async {
    if (busy) return;
    setState(() {
      busy = true;
      status = 'Scanning nearby bots over Bluetooth…';
    });
    try {
      BleProvisioning.wifiPayload(ssid.text, password.text);
      final ble = BleProvisioning();
      final devices = await ble.scan();
      if (!mounted) return;
      if (devices.isEmpty) {
        throw StateError(
            'No bot in setup mode. Follow the recovery steps above, then scan again.');
      }
      final selected = await showDialog<ScanResult>(
          context: context,
          builder: (context) => SimpleDialog(
                title: const Text('Choose your bot'),
                children: devices
                    .map((result) => SimpleDialogOption(
                          onPressed: () => Navigator.pop(context, result),
                          child: Text(
                              '${result.device.platformName.isEmpty ? 'Pico Assistant' : result.device.platformName}\n${result.device.remoteId}'),
                        ))
                    .toList(),
              ));
      if (!mounted) return;
      if (selected == null) {
        setState(() => status = 'No settings changed.');
        return;
      }
      setState(() => status = 'Saving Wi-Fi to the selected bot…');
      await ble.provisionWifi(selected.device, ssid.text, password.text);
      if (!mounted) return;
      password.clear();
      setState(() => status =
          'Wi-Fi saved. The bot is restarting. Reconnect your phone to home Wi-Fi and check the Dashboard.');
    } catch (error) {
      if (mounted) setState(() => status = '$error');
    } finally {
      if (mounted) setState(() => busy = false);
    }
  }

  @override
  Widget build(BuildContext context) => Scaffold(
        appBar: AppBar(title: const Text('Bot Wi-Fi setup')),
        body: ListView(padding: const EdgeInsets.all(16), children: [
          Text('Recover an offline bot',
              style: Theme.of(context).textTheme.titleLarge),
          const SizedBox(height: 12),
          const Text(
              '1. With firmware 1.6+, leave the bot powered on for 60 seconds without Wi-Fi, or hold its touch pad for 5 seconds while offline.\n\n'
              'Older firmware: hold the touch pad while powering on, for 5 seconds.\n\n'
              '2. Read the PICO-xxxxxx hotspot name and password from its screen. Join that hotspot and stay connected even when Android says “No Internet”.\n\n'
              '3. Open the setup page and save your new home Wi-Fi password. Cloud and PC settings are kept. Rejoin home Wi-Fi after the bot restarts.'),
          const SizedBox(height: 16),
          FilledButton.icon(
              onPressed: () => open('wifiSettings'),
              icon: const Icon(Icons.wifi),
              label: const Text('Open phone Wi-Fi settings')),
          OutlinedButton(
              onPressed: () => open('setupPortal'),
              child: const Text('Open bot setup page')),
          const SelectableText('http://192.168.4.1'),
          const Divider(height: 32),
          Text('Or update over Bluetooth',
              style: Theme.of(context).textTheme.titleLarge),
          const Text(
              'Firmware 1.6+ required. The bot must be in setup mode. This sends only Wi-Fi settings; it does not replace the bot’s cloud account. For first-time setup, use the browser page.'),
          const SizedBox(height: 12),
          TextField(
              controller: ssid,
              enabled: !busy,
              decoration:
                  const InputDecoration(labelText: 'Home Wi-Fi name (SSID)')),
          TextField(
              controller: password,
              enabled: !busy,
              obscureText: true,
              decoration:
                  const InputDecoration(labelText: 'New Wi-Fi password')),
          const SizedBox(height: 12),
          FilledButton(
              onPressed: busy ? null : sendWifi,
              child: Text(busy ? 'Working…' : 'Find bot and update Wi-Fi')),
          const SizedBox(height: 12),
          if (status.isNotEmpty) Text(status),
        ]),
      );

  @override
  void dispose() {
    ssid.dispose();
    password.dispose();
    super.dispose();
  }
}

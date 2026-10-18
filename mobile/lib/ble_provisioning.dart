import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

import 'models.dart';

class BleProvisioning {
  static final serviceUuid = Guid('b7a70001-ec76-4b4f-9a06-8b865ce25170');
  static final configUuid = Guid('b7a70002-ec76-4b4f-9a06-8b865ce25170');
  static final statusUuid = Guid('b7a70003-ec76-4b4f-9a06-8b865ce25170');
  static final infoUuid = Guid('b7a70004-ec76-4b4f-9a06-8b865ce25170');

  Future<List<ScanResult>> scan() async {
    if (Platform.isAndroid) {
      final permissions = await [
        Permission.bluetoothScan,
        Permission.bluetoothConnect,
        Permission.locationWhenInUse,
      ].request();
      final modernBluetooth =
          permissions[Permission.bluetoothScan]?.isGranted == true &&
              permissions[Permission.bluetoothConnect]?.isGranted == true;
      final legacyBluetooth =
          permissions[Permission.locationWhenInUse]?.isGranted == true;
      if (!modernBluetooth && !legacyBluetooth) {
        throw StateError(
            'Bluetooth permission is required to find Pico Assistant.');
      }
    }
    await FlutterBluePlus.stopScan();
    await FlutterBluePlus.startScan(
        withServices: [serviceUuid], timeout: const Duration(seconds: 8));
    await FlutterBluePlus.isScanning.where((value) => !value).first;
    // The service UUID identifies the bot. Some Android devices omit/cache
    // the advertised name, which must not hide a valid recovery device.
    return FlutterBluePlus.lastScanResults.toList(growable: false);
  }

  static Map<String, String> wifiPayload(String ssid, String password) {
    if (ssid.isEmpty || utf8.encode(ssid).length > 32) {
      throw ArgumentError('Wi-Fi name must be 1–32 bytes.');
    }
    if (password.isNotEmpty &&
        (utf8.encode(password).length < 8 ||
            utf8.encode(password).length > 63)) {
      throw ArgumentError(
          'Wi-Fi password must be 8–63 bytes, or empty for an open network.');
    }
    return {'wifiSsid': ssid, 'wifiPassword': password};
  }

  Future<String> provisionWifi(
          BluetoothDevice device, String ssid, String password) =>
      provision(device, const AppSettings(), ssid, password, wifiOnly: true);

  Future<String> provision(BluetoothDevice device, AppSettings settings,
      String wifiSsid, String wifiPassword,
      {bool wifiOnly = false}) async {
    final wifi = wifiPayload(wifiSsid, wifiPassword);
    StreamSubscription<List<int>>? subscription;
    await device.connect(timeout: const Duration(seconds: 15));
    try {
      if (device.mtuNow < 247) {
        try {
          await device.requestMtu(512);
        } catch (_) {/* iOS negotiates automatically. */}
      }
      final services = await device.discoverServices();
      final service = services.firstWhere((value) => value.uuid == serviceUuid);
      if (wifiOnly) {
        final info = service.characteristics.where((value) => value.uuid == infoUuid);
        if (info.isEmpty || utf8.decode(await info.first.read()) != 'wifi-only-v1') {
          throw StateError('Wi-Fi-only Bluetooth setup needs firmware 1.6+. Use the browser setup page for this bot. No settings were sent.');
        }
      }
      final config = service.characteristics
          .firstWhere((value) => value.uuid == configUuid);
      final status = service.characteristics
          .firstWhere((value) => value.uuid == statusUuid);
      await status.setNotifyValue(true);
      final response = Completer<String>();
      subscription = status.onValueReceived.listen((bytes) {
        final value = utf8.decode(bytes);
        if (value != 'receiving' &&
            value != 'waiting' &&
            !response.isCompleted) {
          response.complete(value);
        }
      });

      final payload = utf8.encode(jsonEncode({
        ...wifi,
        if (!wifiOnly) ...{
          'deviceId': settings.deviceId,
          'mqttHost': settings.host,
          'mqttPort': settings.port,
          'mqttUsername': settings.username,
          'mqttPassword': settings.password,
          'pcMac': settings.pcMac,
          'broadcastIp': settings.broadcastIp,
          'rootCa': settings.rootCa,
        },
      }));
      await config.write(utf8.encode('BEGIN'), withoutResponse: false);
      final chunkSize = (device.mtuNow - 5).clamp(20, 400).toInt();
      for (var offset = 0; offset < payload.length; offset += chunkSize) {
        final end = (offset + chunkSize).clamp(0, payload.length);
        await config.write(payload.sublist(offset, end),
            withoutResponse: false);
      }
      await config.write(utf8.encode('END'), withoutResponse: false);
      final result = await response.future.timeout(const Duration(seconds: 15));
      if (result != 'saved-rebooting') throw StateError(result);
      return result;
    } finally {
      await subscription?.cancel();
      await device.disconnect();
    }
  }
}

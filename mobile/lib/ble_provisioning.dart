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
        throw StateError('Bluetooth permission is required to find PC Bot.');
      }
    }
    await FlutterBluePlus.stopScan();
    await FlutterBluePlus.startScan(withServices: [serviceUuid], timeout: const Duration(seconds: 8));
    await FlutterBluePlus.isScanning.where((value) => !value).first;
    return FlutterBluePlus.lastScanResults
        .where((result) => result.device.platformName.startsWith('PC Bot'))
        .toList(growable: false);
  }

  Future<String> provision(
      BluetoothDevice device, AppSettings settings, String wifiSsid, String wifiPassword) async {
    await device.connect(timeout: const Duration(seconds: 15));
    try {
      if (device.mtuNow < 247) {
        try { await device.requestMtu(512); } catch (_) { /* iOS negotiates automatically. */ }
      }
      final services = await device.discoverServices();
      final service = services.firstWhere((value) => value.uuid == serviceUuid);
      final config = service.characteristics.firstWhere((value) => value.uuid == configUuid);
      final status = service.characteristics.firstWhere((value) => value.uuid == statusUuid);
      await status.setNotifyValue(true);
      final response = status.onValueReceived
          .map(utf8.decode)
          .firstWhere((value) => value != 'receiving')
          .timeout(const Duration(seconds: 15));

      final payload = utf8.encode(jsonEncode({
        'deviceId': settings.deviceId,
        'wifiSsid': wifiSsid,
        'wifiPassword': wifiPassword,
        'mqttHost': settings.host,
        'mqttPort': settings.port,
        'mqttUsername': settings.username,
        'mqttPassword': settings.password,
        'pcMac': settings.pcMac,
        'broadcastIp': settings.broadcastIp,
        'rootCa': settings.rootCa,
      }));
      await config.write(utf8.encode('BEGIN'), withoutResponse: false);
      final chunkSize = (device.mtuNow - 5).clamp(20, 400).toInt();
      for (var offset = 0; offset < payload.length; offset += chunkSize) {
        final end = (offset + chunkSize).clamp(0, payload.length);
        await config.write(payload.sublist(offset, end), withoutResponse: false);
      }
      await config.write(utf8.encode('END'), withoutResponse: false);
      return await response;
    } finally {
      await device.disconnect();
    }
  }
}

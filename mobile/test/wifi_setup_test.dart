import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pc_bot/ble_provisioning.dart';
import 'package:pc_bot/bot_wifi_setup.dart';

void main() {
  test('Wi-Fi-only recovery never carries cloud credentials', () {
    final payload = BleProvisioning.wifiPayload('New network', 'new-password');
    expect(
        payload, {'wifiSsid': 'New network', 'wifiPassword': 'new-password'});
    expect(payload.containsKey('mqttPassword'), isFalse);
    expect(payload.containsKey('deviceId'), isFalse);
  });

  test('Wi-Fi validation checks byte lengths and permits open networks', () {
    expect(
        () => BleProvisioning.wifiPayload('', 'password'), throwsArgumentError);
    expect(() => BleProvisioning.wifiPayload('a' * 33, 'password'),
        throwsArgumentError);
    expect(() => BleProvisioning.wifiPayload('network', 'short'),
        throwsArgumentError);
    expect(() => BleProvisioning.wifiPayload('network', 'a' * 64),
        throwsArgumentError);
    expect(BleProvisioning.wifiPayload('open', '')['wifiPassword'], '');
  });

  testWidgets('recovery screen works without a cloud controller',
      (tester) async {
    await tester.pumpWidget(const MaterialApp(home: BotWifiSetupScreen()));
    expect(find.text('Bot Wi-Fi setup'), findsOneWidget);
    expect(find.text('Open phone Wi-Fi settings'), findsOneWidget);
    expect(find.text('Open bot setup page'), findsOneWidget);
    expect(find.text('http://192.168.4.1'), findsOneWidget);
  });
}

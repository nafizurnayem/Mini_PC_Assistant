import 'package:flutter_test/flutter_test.dart';
import 'package:pc_bot/models.dart';
import 'package:pc_bot/mqtt_service.dart';

void main() {
  test('settings round-trip without losing broker or WOL fields', () {
    const settings = AppSettings(
      deviceId: 'desktop-01',
      host: 'broker.example',
      username: 'mobile',
      password: 'secret',
      pcMac: 'AA:BB:CC:DD:EE:FF',
      broadcastIp: '192.168.1.255',
      rootCa: 'certificate',
    );
    final restored = AppSettings.fromJson(settings.toJson());
    expect(restored.rootTopic, 'pcbot/v1/desktop-01');
    expect(restored.host, settings.host);
    expect(restored.pcMac, settings.pcMac);
    expect(restored.rootCa, settings.rootCa);
  });

  test('telemetry parser normalizes numbers and storage', () {
    final value = Telemetry.fromPayload('''
      {
        "timestamp":"2026-07-26T10:00:00Z",
        "cpu":{"temperatureC":55,"loadPercent":20.5},
        "gpu":{"temperatureC":60.2,"loadPercent":30},
        "memory":{"usedPercent":42},
        "network":{"downloadMbps":12.3,"uploadMbps":4.5},
        "storage":[{"name":"C:","usedPercent":50}]
      }
    ''');
    expect(value.cpuTemp, 55.0);
    expect(value.gpuLoad, 30.0);
    expect(value.storage.single['name'], 'C:');
  });

  test('command timestamps use strict UTC seconds', () {
    expect(
      formatUtcSeconds(DateTime.utc(2026, 7, 26, 10, 9, 8, 123)),
      '2026-07-26T10:09:08Z',
    );
  });
}

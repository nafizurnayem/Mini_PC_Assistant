import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pc_bot/app_controller.dart';
import 'package:pc_bot/main.dart';
import 'package:pc_bot/models.dart';
import 'package:pc_bot/mqtt_service.dart';
import 'package:pc_bot/secure_settings.dart';

class MemorySettings extends SecureSettings {
  @override
  Future<AppSettings> load() async => const AppSettings();
}

class FakeMqtt extends MqttService {
  final incoming = StreamController<TopicMessage>.broadcast(sync: true);
  final changes = StreamController<bool>.broadcast(sync: true);
  final telemetrySubscriptions = <bool>[];

  @override
  Stream<TopicMessage> get messages => incoming.stream;
  @override
  Stream<bool> get connection => changes.stream;
  @override
  void setTelemetryEnabled(bool enabled) => telemetrySubscriptions.add(enabled);

  void sendLoad(int load) => incoming.add(TopicMessage(
      'pcbot/v1/test/telemetry/system',
      jsonEncode({'cpu': {'loadPercent': load}})));

  @override
  Future<void> dispose() async {
    await incoming.close();
    await changes.close();
    await super.dispose();
  }
}

void main() {
  testWidgets('telemetry bursts render only the latest sample once per second',
      (tester) async {
    final mqtt = FakeMqtt();
    final controller = AppController(MemorySettings(), mqtt);
    await controller.initialize();
    var updates = 0;
    controller.addListener(() => updates++);
    for (var i = 0; i < 100; i++) {
      mqtt.sendLoad(i);
    }
    await tester.pump(const Duration(milliseconds: 999));
    expect(updates, 0);
    await tester.pump(const Duration(milliseconds: 1));
    expect(updates, 1);
    expect(controller.telemetry!.cpuLoad, 99);
    expect(controller.history, hasLength(1));
    controller.dispose();
  });

  testWidgets('background pauses telemetry and resume accepts fresh data',
      (tester) async {
    final mqtt = FakeMqtt();
    final controller = AppController(MemorySettings(), mqtt);
    await controller.initialize();
    mqtt.sendLoad(10);
    controller.setForeground(false);
    controller.setForeground(false);
    mqtt.sendLoad(20);
    await tester.pump(const Duration(seconds: 2));
    expect(controller.telemetry, isNull);
    expect(mqtt.telemetrySubscriptions, [false]);
    controller.setForeground(true);
    mqtt.sendLoad(30);
    await tester.pump(const Duration(seconds: 1));
    expect(controller.telemetry!.cpuLoad, 30);
    expect(mqtt.telemetrySubscriptions, [false, true]);
    controller.dispose();
  });

  testWidgets('status stays immediate and unused topics do not redraw',
      (tester) async {
    final mqtt = FakeMqtt();
    final controller = AppController(MemorySettings(), mqtt);
    await controller.initialize();
    var updates = 0;
    controller.addListener(() => updates++);
    mqtt.incoming.add(const TopicMessage('pcbot/v1/test/event/ack', '{}'));
    expect(updates, 0);
    mqtt.incoming.add(const TopicMessage(
        'pcbot/v1/test/state/pc', '{"status":"online"}'));
    expect(controller.pcOnline, isTrue);
    expect(updates, 1);
    mqtt.sendLoad(10);
    controller.dispose();
    await tester.pump(const Duration(seconds: 2));
    expect(updates, 1);
  });

  testWidgets('metric values render immediately without animation', (tester) async {
    await tester.pumpWidget(const MaterialApp(
      home: Scaffold(body: MetricCard('CPU', 42, '°C', 12, '% load', width: 180)),
    ));
    expect(find.text('42.0 °C'), findsOneWidget);
    expect(find.byType(TweenAnimationBuilder<double>), findsNothing);
  });
}

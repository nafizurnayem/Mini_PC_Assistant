import 'dart:async';
import 'dart:convert';

import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import 'package:uuid/uuid.dart';

import 'models.dart';

class TopicMessage {
  const TopicMessage(this.topic, this.payload);
  final String topic;
  final String payload;
}

String formatUtcSeconds(DateTime value) {
  final utc = value.toUtc();
  String two(int number) => number.toString().padLeft(2, '0');
  return '${utc.year.toString().padLeft(4, '0')}-${two(utc.month)}-'
      '${two(utc.day)}T${two(utc.hour)}:${two(utc.minute)}:${two(utc.second)}Z';
}

class MqttService {
  MqttServerClient? _client;
  StreamSubscription? _subscription;
  final _messages = StreamController<TopicMessage>.broadcast();
  final _connection = StreamController<bool>.broadcast();

  Stream<TopicMessage> get messages => _messages.stream;
  Stream<bool> get connection => _connection.stream;
  bool get connected => _client?.connectionStatus?.state == MqttConnectionState.connected;

  Future<void> connect(AppSettings settings) async {
    await disconnect();
    final client = MqttServerClient.withPort(
      settings.host,
      'pcbot-mobile-${const Uuid().v4()}',
      settings.port,
    )
      ..secure = true
      ..keepAlivePeriod = 30
      ..autoReconnect = true
      ..resubscribeOnAutoReconnect = true
      ..logging(on: false)
      ..onConnected = () {
        _connection.add(true);
      }
      ..onDisconnected = () {
        _connection.add(false);
      }
      ..connectionMessage = MqttConnectMessage()
          .authenticateAs(settings.username, settings.password)
          .startClean()
          .withWillTopic('${settings.rootTopic}/state/mobile')
          .withWillMessage('{"status":"offline"}')
          .withWillQos(MqttQos.atLeastOnce);
    _client = client;
    final result = await client.connect();
    if (result?.state != MqttConnectionState.connected) {
      throw StateError('MQTT connection failed: ${result?.returnCode}');
    }
    for (final suffix in ['state/pc', 'state/bot', 'state/speedtest', 'telemetry/system', 'event/alert', 'event/ack']) {
      client.subscribe('${settings.rootTopic}/$suffix', MqttQos.atLeastOnce);
    }
    _subscription = client.updates?.listen((events) {
      for (final event in events) {
        final message = event.payload as MqttPublishMessage;
        final payload = MqttPublishPayload.bytesToStringAsString(message.payload.message);
        _messages.add(TopicMessage(event.topic, payload));
      }
    });
  }

  void publishCommand(AppSettings settings, String topicSuffix, String action) {
    final client = _client;
    if (client == null || !connected) throw StateError('MQTT is not connected');
    final now = DateTime.now().toUtc();
    final payload = jsonEncode({
      'commandId': const Uuid().v4(),
      'source': 'mobile',
      'action': action,
      'issuedAt': formatUtcSeconds(now),
      'expiresAt': formatUtcSeconds(now.add(const Duration(seconds: 30))),
    });
    final builder = MqttClientPayloadBuilder()..addUTF8String(payload);
    client.publishMessage('${settings.rootTopic}/$topicSuffix', MqttQos.atLeastOnce,
        builder.payload!, retain: false);
  }

  Future<void> disconnect() async {
    await _subscription?.cancel();
    _subscription = null;
    _client?.disconnect();
    _client = null;
    _connection.add(false);
  }

  Future<void> dispose() async {
    await disconnect();
    await _messages.close();
    await _connection.close();
  }
}

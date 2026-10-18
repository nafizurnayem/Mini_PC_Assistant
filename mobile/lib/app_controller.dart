import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';

import 'models.dart';
import 'mqtt_service.dart';
import 'secure_settings.dart';

class AppController extends ChangeNotifier {
  AppController(this._store, this._mqtt);

  final SecureSettings _store;
  final MqttService _mqtt;
  StreamSubscription? _messages;
  StreamSubscription? _connection;
  Timer? _telemetryTimer;
  String? _pendingTelemetry;
  bool _foreground = true;

  void setForeground(bool foreground) {
    if (_foreground == foreground) return;
    _foreground = foreground;
    _mqtt.setTelemetryEnabled(foreground);
    if (!foreground) {
      _telemetryTimer?.cancel();
      _telemetryTimer = null;
      _pendingTelemetry = null;
    }
  }

  void _queueTelemetry(String payload) {
    if (!_foreground) return;
    _pendingTelemetry = payload;
    // Parse and repaint at most once a second, even after broker bursts.
    _telemetryTimer ??= Timer(const Duration(seconds: 1), () {
      _telemetryTimer = null;
      final payload = _pendingTelemetry;
      _pendingTelemetry = null;
      if (payload == null) return;
      try {
        telemetry = Telemetry.fromPayload(payload);
        history.insert(0, telemetry!);
        if (history.length > 60) history.removeLast();
      } catch (exception) {
        error = 'Invalid broker message: $exception';
      }
      notifyListeners();
    });
  }

  AppSettings settings = const AppSettings();
  Telemetry? telemetry;
  SpeedResult? speedResult;
  final List<Telemetry> history = [];
  final List<Map<String, dynamic>> alerts = [];
  bool connected = false;
  bool pcOnline = false;
  bool botOnline = false;
  String? error;

  Future<void> initialize() async {
    settings = await _store.load();
    _messages = _mqtt.messages.listen(_onMessage);
    _connection = _mqtt.connection.listen((value) {
      connected = value;
      notifyListeners();
    });
    if (settings.mqttReady) await reconnect();
    notifyListeners();
  }

  Future<void> saveSettings(AppSettings value) async {
    settings = value;
    await _store.save(value);
    await reconnect();
  }

  Future<void> reconnect() async {
    error = null;
    try {
      await _mqtt.connect(settings);
    } catch (exception) {
      error = exception.toString();
      connected = false;
    }
    notifyListeners();
  }

  void power(bool on) => _mqtt.publishCommand(settings, 'command/power', on ? 'on' : 'off');
  void runSpeedTest() => _mqtt.publishCommand(settings, 'command/speedtest', 'run');
  void resetWifi() => _mqtt.publishCommand(settings, 'command/reset', 'wifi');
  void factoryReset() => _mqtt.publishCommand(settings, 'command/reset', 'factory');

  void _onMessage(TopicMessage message) {
    try {
      if (message.topic.endsWith('/state/pc')) {
        final json = jsonDecode(message.payload) as Map<String, dynamic>;
        pcOnline = json['status'] == 'online';
      } else if (message.topic.endsWith('/state/bot')) {
        final json = jsonDecode(message.payload) as Map<String, dynamic>;
        botOnline = json['status'] == 'online';
      } else if (message.topic.endsWith('/telemetry/system')) {
        _queueTelemetry(message.payload);
        return;
      } else if (message.topic.endsWith('/state/speedtest')) {
        speedResult = SpeedResult.fromPayload(message.payload);
      } else if (message.topic.endsWith('/event/alert')) {
        alerts.insert(0, jsonDecode(message.payload) as Map<String, dynamic>);
        if (alerts.length > 50) alerts.removeLast();
      } else {
        return;
      }
      notifyListeners();
    } catch (exception) {
      error = 'Invalid broker message: $exception';
      notifyListeners();
    }
  }

  @override
  void dispose() {
    _telemetryTimer?.cancel();
    _messages?.cancel();
    _connection?.cancel();
    _mqtt.dispose();
    super.dispose();
  }
}


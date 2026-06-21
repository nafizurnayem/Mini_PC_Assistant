import 'dart:convert';

class AppSettings {
  const AppSettings({
    this.deviceId = 'desktop-01',
    this.host = '',
    this.port = 8883,
    this.username = '',
    this.password = '',
    this.pcMac = '',
    this.broadcastIp = '255.255.255.255',
    this.rootCa = '',
  });

  final String deviceId;
  final String host;
  final int port;
  final String username;
  final String password;
  final String pcMac;
  final String broadcastIp;
  final String rootCa;

  String get rootTopic => 'pcbot/v1/$deviceId';
  bool get mqttReady => host.isNotEmpty && username.isNotEmpty && password.isNotEmpty;

  Map<String, dynamic> toJson() => {
        'deviceId': deviceId,
        'host': host,
        'port': port,
        'username': username,
        'password': password,
        'pcMac': pcMac,
        'broadcastIp': broadcastIp,
        'rootCa': rootCa,
      };

  factory AppSettings.fromJson(Map<String, dynamic> json) => AppSettings(
        deviceId: json['deviceId'] as String? ?? 'desktop-01',
        host: json['host'] as String? ?? '',
        port: json['port'] as int? ?? 8883,
        username: json['username'] as String? ?? '',
        password: json['password'] as String? ?? '',
        pcMac: json['pcMac'] as String? ?? '',
        broadcastIp: json['broadcastIp'] as String? ?? '255.255.255.255',
        rootCa: json['rootCa'] as String? ?? '',
      );

  AppSettings copyWith({
    String? deviceId,
    String? host,
    int? port,
    String? username,
    String? password,
    String? pcMac,
    String? broadcastIp,
    String? rootCa,
  }) =>
      AppSettings(
        deviceId: deviceId ?? this.deviceId,
        host: host ?? this.host,
        port: port ?? this.port,
        username: username ?? this.username,
        password: password ?? this.password,
        pcMac: pcMac ?? this.pcMac,
        broadcastIp: broadcastIp ?? this.broadcastIp,
        rootCa: rootCa ?? this.rootCa,
      );
}

class Telemetry {
  const Telemetry({
    required this.timestamp,
    this.cpuTemp,
    this.cpuLoad,
    this.gpuTemp,
    this.gpuLoad,
    this.memoryUsed,
    this.downloadMbps,
    this.uploadMbps,
    this.storage = const [],
  });

  final DateTime timestamp;
  final double? cpuTemp;
  final double? cpuLoad;
  final double? gpuTemp;
  final double? gpuLoad;
  final double? memoryUsed;
  final double? downloadMbps;
  final double? uploadMbps;
  final List<Map<String, dynamic>> storage;

  factory Telemetry.fromPayload(String payload) {
    final json = jsonDecode(payload) as Map<String, dynamic>;
    double? number(dynamic value) => value is num ? value.toDouble() : null;
    return Telemetry(
      timestamp: DateTime.tryParse(json['timestamp'] as String? ?? '') ?? DateTime.now(),
      cpuTemp: number(json['cpu']?['temperatureC']),
      cpuLoad: number(json['cpu']?['loadPercent']),
      gpuTemp: number(json['gpu']?['temperatureC']),
      gpuLoad: number(json['gpu']?['loadPercent']),
      memoryUsed: number(json['memory']?['usedPercent']),
      downloadMbps: number(json['network']?['downloadMbps']),
      uploadMbps: number(json['network']?['uploadMbps']),
      storage: (json['storage'] as List<dynamic>? ?? const [])
          .whereType<Map<String, dynamic>>()
          .toList(growable: false),
    );
  }
}

class SpeedResult {
  const SpeedResult(this.status, this.download, this.upload, this.ping, this.error);
  final String status;
  final double? download;
  final double? upload;
  final double? ping;
  final String? error;

  factory SpeedResult.fromPayload(String payload) {
    final json = jsonDecode(payload) as Map<String, dynamic>;
    double? n(dynamic value) => value is num ? value.toDouble() : null;
    return SpeedResult(json['status'] as String? ?? 'unknown', n(json['downloadMbps']),
        n(json['uploadMbps']), n(json['pingMs']), json['error'] as String?);
  }
}


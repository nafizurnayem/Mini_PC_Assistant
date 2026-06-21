import 'dart:convert';

import 'package:flutter_secure_storage/flutter_secure_storage.dart';

import 'models.dart';

class SecureSettings {
  static const _key = 'pcbot.settings.v1';
  final FlutterSecureStorage _storage = const FlutterSecureStorage(
    aOptions: AndroidOptions(encryptedSharedPreferences: true),
  );

  Future<AppSettings> load() async {
    final value = await _storage.read(key: _key);
    if (value == null) return const AppSettings();
    return AppSettings.fromJson(jsonDecode(value) as Map<String, dynamic>);
  }

  Future<void> save(AppSettings settings) =>
      _storage.write(key: _key, value: jsonEncode(settings.toJson()));

  Future<void> clear() => _storage.delete(key: _key);
}


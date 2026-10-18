package com.pcbot.pc_bot

import android.content.Intent
import android.net.Uri
import android.provider.Settings
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "pcbot/setup")
            .setMethodCallHandler { call, result ->
                val intent = when (call.method) {
                    "wifiSettings" -> Intent(Settings.ACTION_WIFI_SETTINGS)
                    "setupPortal" -> Intent(Intent.ACTION_VIEW, Uri.parse("http://192.168.4.1"))
                    else -> null
                }
                if (intent == null) {
                    result.notImplemented()
                } else {
                    try {
                        startActivity(intent)
                        result.success(null)
                    } catch (error: Exception) {
                        result.error("unavailable", "Open Wi-Fi settings or http://192.168.4.1 manually.", null)
                    }
                }
            }
    }
}

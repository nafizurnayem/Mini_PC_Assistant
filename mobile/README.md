# Mobile app

Run `./bootstrap.ps1` after installing Flutter. It generates Android/iOS host
projects around committed Dart source.

Android manifest permissions required:

```xml
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" android:usesPermissionFlags="neverForLocation" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.INTERNET" />
```

iOS `Info.plist` keys required:

```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>Provision the PC Assistant Bot over Bluetooth.</string>
<key>NSLocalNetworkUsageDescription</key>
<string>Configure the bot on your home network.</string>
```

iOS signing and physical BLE testing require macOS, Xcode, and an Apple
developer identity. MQTT credentials remain in Keychain/Android Keystore.

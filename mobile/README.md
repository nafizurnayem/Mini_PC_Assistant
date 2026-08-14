# Mobile app

Run `./bootstrap.ps1` after installing Flutter. Android and iOS host projects,
permissions, tests, and the gaming-style responsive UI are committed.

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

Build Android with `flutter build apk --release` after installing Android Studio
or an Android SDK accepted by `flutter doctor`. Before publishing, copy
`android/key.properties.example` to `android/key.properties`, create a private
release keystore, and replace every placeholder. Without that file Gradle uses
debug signing for local release-mode testing only. The app supports live telemetry,
alert/history views, Wake-on-LAN and graceful-shutdown confirmation, speed
tests, encrypted BLE provisioning, Wi-Fi recovery, and protected factory reset.

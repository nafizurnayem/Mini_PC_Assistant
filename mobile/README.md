# Mobile app

## Lightweight Android build (including Oreo)

Version 0.2.0 supports Android 7.0+ (API 24), including Android 8.0/8.1 Oreo.
Build separate phone APKs with `bash build-lite.sh` (Flutter must be on PATH),
or run:

```sh
flutter build apk --release --split-per-abi --target-platform android-arm,android-arm64
```

The local 0.2.0 release APKs are 14.9 MB (32-bit ARM) and 17.6 MB (64-bit ARM),
compared with the previous 51.1 MB universal APK. RAM use has not yet been measured
on the target Vivo Y1s (2 GB RAM).

Install **one** APK from `build/app/outputs/flutter-apk/`:

- `app-armeabi-v7a-release.apk`: phones running 32-bit ARM Android.
- `app-arm64-v8a-release.apk`: phones running 64-bit ARM Android.

Android version alone does not identify the processor ABI. With USB debugging
enabled, `adb shell getprop ro.product.cpu.abilist` lists supported ABIs.
Some older phones have a 64-bit chip but run 32-bit Android; use the ABI reported
by Android. These are complete standalone APKs, not parts that install together.

The dashboard displays readings without number or page-transition animations.
Telemetry bursts are coalesced into one latest reading per second. While the app
is in the background, it unsubscribes from telemetry and cancels pending updates;
returning to the app resubscribes. The cloud connection remains available for
status and alerts. History keeps up to 60 displayed samples and excludes time
spent in the background. Code/resource shrinking and icon tree shaking remain
enabled; an unused shared-preferences plugin has been removed. Cloud control,
speed tests, Bluetooth and browser Wi-Fi setup remain available.

For Bluetooth scanning on Oreo, grant location permission and turn on the phone's
Location and Bluetooth switches. The browser/hotspot setup is also available.
Actual performance and Bluetooth behavior still need testing on the old phone.


**Bot Wi-Fi setup** is available from the top-bar Wi-Fi icon and Settings,
including when the bot/cloud is offline. The recovery screen explains the
physical gesture, opens Android Wi-Fi settings and the local browser setup
page, and can send Wi-Fi-only settings over Bluetooth on firmware 1.6+.
It asks you to choose the bot and preserves its existing cloud credentials.
First-time setup still uses the browser's full configuration page. iOS users
can open Wi-Fi settings and the displayed URL manually.

After changing a router password, firmware 1.6 opens its protected setup
hotspot after 60 seconds offline. On older firmware, hold the bot's touch pad
while powering on for five seconds, then join the hotspot shown on its screen.
Keep the phone connected when Android reports that the hotspot has no Internet.

The Ubuntu-built local APK uses this machine's development signing key, which
differs from the previous Windows-built APK. Android cannot update an existing
installation signed with the other key in place. Reuse the original signing
key for a seamless update; otherwise record your connection settings before
manually reinstalling. Uninstalling clears the app's saved credentials.

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

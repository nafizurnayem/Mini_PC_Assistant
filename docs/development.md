# Development

Application code stays in its platform folder. Shared MQTT messages are defined
in `contracts/`. Run the commands below from the repository root unless noted.

## Checks

Shared configuration requires Node.js 20.11+ and Python with PyYAML:

```bash
npm test
npm run validate
python3 tools/validate-yaml.py
```

Linux behavior tests (after `cd linux && bash setup.sh`):

```bash
cd linux
bash verify.sh
```

Firmware build and recovery behavior check (PlatformIO and a C++ compiler):

```bash
python3 -m platformio run -d firmware
g++ -std=c++11 tests/wifi-recovery.cpp -o /tmp/pico-wifi-recovery-test
/tmp/pico-wifi-recovery-test
```

Mobile app (Flutter and platform SDKs):

```bash
cd mobile
flutter pub get
flutter analyze
flutter test
```

For Windows builds and tests, use [the Windows guide](../windows/README.md).
With Node.js, Python/PyYAML, PlatformIO, .NET 8, and Flutter on your PATH,
`powershell -File tools/verify.ps1` runs the Windows development checks.
The enclosure's checks are described in [the case guide](../hardware/case/README.md).

## Git uploads

Commit source, tests, documentation, configuration examples, and
`mobile/pubspec.lock`. The ignore files exclude local SDKs, caches, build
outputs, credentials, signing keys, and editor settings. Dependencies and build
outputs are recreated by each component's setup/build commands.

Before committing:

```bash
git status --short
git diff --check
git diff
```

Review newly added files too: ignore rules cannot detect every secret or remove
files already tracked by Git. Enter real credentials in the app or bot setup
page, and keep private Home Assistant configuration out of commits. Share APKs
and other compiled downloads as release attachments instead of source files.

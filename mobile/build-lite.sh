#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
# Separate standalone APKs: install the one matching the phone's Android ABI.
flutter build apk --release --split-per-abi --target-platform android-arm,android-arm64 "$@"

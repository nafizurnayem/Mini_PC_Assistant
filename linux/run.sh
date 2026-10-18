#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
pcbot_python="${PICO_PYTHON:-$PWD/.venv/bin/python}"
if [[ ! -x "$pcbot_python" ]]; then
  echo 'Run bash setup.sh first.' >&2
  exit 1
fi
exec "$pcbot_python" -m pcbot_linux "$@"

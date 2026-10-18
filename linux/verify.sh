#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
pcbot_python="${PICO_PYTHON:-$PWD/.venv/bin/python}"
"$pcbot_python" -m unittest discover -s tests -v
"$pcbot_python" -m pcbot_linux snapshot >/dev/null
"$pcbot_python" -m pcbot_linux diagnose
bash -n setup.sh run.sh install.sh verify.sh

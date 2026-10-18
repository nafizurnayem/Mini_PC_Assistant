#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
if ! python3 -c 'import venv, tkinter' >/dev/null 2>&1; then
  echo 'Install Ubuntu dependencies first: sudo apt install python3-venv python3-tk pulseaudio-utils playerctl'
  exit 1
fi
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
echo 'Ready. Launch with: bash run.sh'

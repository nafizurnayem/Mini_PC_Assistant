"""Install for the current desktop user, without sudo or a dependency on the checkout."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import venv

UNIT = """[Unit]
Description=Pico Assistant Linux MQTT agent
After=graphical-session-pre.target
PartOf=graphical-session.target

[Service]
Type=simple
WorkingDirectory=%h/.local/share/pico-assistant
ExecStart="%h/.local/share/pico-assistant/venv/bin/python" -m pcbot_linux agent
Restart=on-failure
RestartSec=10
TimeoutStopSec=20
UMask=0077

[Install]
WantedBy=graphical-session.target
"""


def desktop_quote(value):
    return '"' + str(value).replace('\\', '\\\\').replace('"', '\\"').replace('`', '\\`').replace('$', '\\$').replace('%', '%%') + '"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uninstall", action="store_true", help="Remove launchers and service; preserve settings and application files")
    args = parser.parse_args()
    if os.geteuid() == 0:
        parser.error("Run as your desktop user, without sudo")
    home = Path.home()
    source = Path(__file__).resolve().parent
    destination = home / ".local/share/pico-assistant"
    unit = home / ".config/systemd/user/pico-assistant.service"
    desktop = home / ".local/share/applications/pico-assistant.desktop"
    launcher = home / ".local/bin/pico-assistant"
    if args.uninstall:
        subprocess.run(["systemctl", "--user", "disable", "--now", "pico-assistant.service"], check=False)
        for path in (unit, desktop, launcher):
            path.unlink(missing_ok=True)
        subprocess.run(["systemctl", "--user", "daemon-reload"], check=True)
        print(f"Service and launchers removed. Settings preserved. Application files remain at {destination}")
        return
    try:
        import tkinter  # noqa: F401
    except ImportError:
        parser.error("Install python3-tk and python3-venv with apt first")
    # Check the user manager before changing an existing installation.
    subprocess.run(["systemctl", "--user", "show-environment"], check=True, stdout=subprocess.DEVNULL)
    if unit.exists():
        subprocess.run(["systemctl", "--user", "stop", "pico-assistant.service"], check=True)
    destination.mkdir(parents=True, exist_ok=True)
    venv.EnvBuilder(with_pip=True).create(destination / "venv")
    python = destination / "venv/bin/python"
    subprocess.run([str(python), "-m", "pip", "install", "-r", str(source / "requirements.txt")], check=True)
    shutil.copytree(source / "pcbot_linux", destination / "pcbot_linux", dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
    for path in (unit, desktop, launcher):
        path.parent.mkdir(parents=True, exist_ok=True)
    unit.write_text(UNIT)
    # Launcher resolves its installation independently of cwd and checkout paths with spaces.
    launcher.write_text('#!/usr/bin/env python3\nimport os\nfrom pathlib import Path\nimport sys\np = Path.home() / ".local/share/pico-assistant"\nos.chdir(p)\nos.execv(str(p / "venv/bin/python"), [str(p / "venv/bin/python"), "-m", "pcbot_linux", *sys.argv[1:]])\n')
    launcher.chmod(0o755)
    desktop.write_text("[Desktop Entry]\nType=Application\nName=Pico Assistant\nComment=Linux cloud and ESP32 control panel\n"
                       f"Exec={desktop_quote(launcher)}\nIcon=computer\nTerminal=false\nCategories=Utility;System;\n")
    subprocess.run(["systemctl", "--user", "daemon-reload"], check=True)
    subprocess.run(["systemctl", "--user", "enable", "pico-assistant.service"], check=True)
    print("Installed. Open Pico Assistant from Applications, save MQTT settings, then click Start agent.")
    print("The agent starts automatically at graphical login. No root service or firewall changes were made.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, subprocess.CalledProcessError) as error:
        sys.exit(f"Installation failed: {error}")

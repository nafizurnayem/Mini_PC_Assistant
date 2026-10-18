"""Install the official Ookla CLI for this user, without sudo or changing apt sources."""
import io
import os
from pathlib import Path
import platform
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

VERSION = "1.2.0"
ARCHITECTURES = {"x86_64": "x86_64", "aarch64": "aarch64", "armv7l": "armhf", "i686": "i386"}


def main():
    if os.geteuid() == 0:
        raise RuntimeError("Run as your desktop user, without sudo")
    architecture = ARCHITECTURES.get(platform.machine())
    if not architecture:
        raise RuntimeError(f"Unsupported architecture: {platform.machine()}. See https://www.speedtest.net/apps/cli")
    url = f"https://install.speedtest.net/app/cli/ookla-speedtest-{VERSION}-linux-{architecture}.tgz"
    print(f"Downloading official Ookla Speedtest {VERSION} for {architecture}…", flush=True)
    with urllib.request.urlopen(url, timeout=60) as response:
        data = response.read(20 * 1024 * 1024 + 1)
    if len(data) > 20 * 1024 * 1024:
        raise RuntimeError("Unexpectedly large download")
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
        members = [m for m in archive.getmembers() if Path(m.name).name == "speedtest" and m.isfile()]
        if len(members) != 1 or members[0].size > 20 * 1024 * 1024:
            raise RuntimeError("Archive does not contain the expected speedtest binary")
        # Read only the expected regular file; never extract arbitrary archive paths.
        with archive.extractfile(members[0]) as stream:
            binary = stream.read()
    destination = Path.home() / ".local/share/pico-assistant/bin"
    destination.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".speedtest-", dir=destination)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(binary)
        os.chmod(temporary, 0o755)
        result = subprocess.run([temporary, "--version"], check=True, capture_output=True, text=True, timeout=10)
        if "ookla" not in (result.stdout + result.stderr).lower():
            raise RuntimeError("Downloaded program did not identify itself as Ookla Speedtest")
        os.replace(temporary, destination / "speedtest")
        print(result.stdout.strip())
        print(f"Installed: {destination / 'speedtest'}")
        print("Keep the app's Speedtest executable set to speedtest. Tests accept Ookla's license/GDPR terms when requested.")
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, tarfile.TarError, subprocess.SubprocessError) as error:
        sys.exit(f"Speedtest installation failed: {error}")

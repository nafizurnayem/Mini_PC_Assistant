"""Wi-Fi-only recovery over the bot's password-protected setup hotspot."""
import json
import urllib.error
import urllib.parse
import urllib.request

SETUP_URL = "http://192.168.4.1"
INSTRUCTIONS = (
    "1. Leave the offline bot powered on for 60 seconds (firmware 1.6+), or hold "
    "its touch pad for 5 seconds while offline.\n"
    "   Older firmware: hold the touch pad while powering on, for 5 seconds.\n"
    "2. Join the PICO-xxxxxx Wi-Fi hotspot using the password on the bot screen. "
    "Stay connected even though it has no Internet.\n"
    "3. Enter your home Wi-Fi name and new password below, or open the setup page.\n"
    "4. After saving, reconnect this PC to your home Wi-Fi. The bot restarts automatically.\n"
    "Wi-Fi-only setup keeps the bot's cloud credentials and PC settings."
)


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise RuntimeError("Unexpected redirect. Connect directly to the bot's PICO setup hotspot.")


def save_wifi(ssid, password):
    if not ssid or len(ssid.encode("utf-8")) > 32:
        raise ValueError("Wi-Fi name must be 1–32 bytes")
    if password and not 8 <= len(password.encode("utf-8")) <= 63:
        raise ValueError("Wi-Fi password must be 8–63 bytes, or empty for an open network")
    # Never send home Wi-Fi credentials through a configured HTTP proxy.
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())

    def request(path, data=None):
        req = urllib.request.Request(SETUP_URL + path, data=data,
                                     headers={"Content-Type": "application/x-www-form-urlencoded"})
        try:
            with opener.open(req, timeout=10) as response:
                body = response.read(16385)
                if len(body) > 16384:
                    raise RuntimeError("Unexpected response from setup device")
                return json.loads(body)
        except urllib.error.HTTPError as error:
            try:
                detail = json.loads(error.read(16384)).get("message", str(error))
            except (ValueError, AttributeError):
                detail = str(error)
            raise RuntimeError(detail) from None
        except (urllib.error.URLError, TimeoutError, OSError) as error:
            raise RuntimeError("Cannot reach the bot. Join its PICO setup hotspot first, then retry.") from error

    info = request("/api/config")
    if not isinstance(info, dict) or not info.get("firmwareVersion"):
        raise RuntimeError("This bot needs firmware 1.6+ for in-app Wi-Fi setup. Use Open setup page for older firmware.")
    if not info.get("wifiOnlyAvailable"):
        raise RuntimeError("The bot needs full setup. Open the setup page and enter its dedicated cloud settings.")
    result = request("/api/wifi", urllib.parse.urlencode({"wifiSsid": ssid, "wifiPassword": password}).encode())
    if not isinstance(result, dict) or result.get("status") != "saved-rebooting":
        raise RuntimeError("The bot did not confirm saving. Open the setup page to check its settings.")
    return "Wi-Fi saved. The bot is restarting; reconnect this PC to your home network."

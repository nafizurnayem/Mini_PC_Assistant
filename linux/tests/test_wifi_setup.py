import io
import json
import unittest
import urllib.error
import urllib.parse
from unittest.mock import Mock, patch

from pcbot_linux.wifi_setup import NoRedirect, save_wifi


def response(value):
    return io.BytesIO(json.dumps(value).encode())


class WifiSetupTests(unittest.TestCase):
    def test_only_wifi_fields_sent_directly_to_hotspot(self):
        opener = Mock()
        opener.open.side_effect = [response({"firmwareVersion": "1.6.0", "wifiOnlyAvailable": True}),
                                   response({"status": "saved-rebooting"})]
        with patch("pcbot_linux.wifi_setup.urllib.request.build_opener", return_value=opener) as factory:
            self.assertIn("Wi-Fi saved", save_wifi("My Wi-Fi", "new password"))
        self.assertEqual(factory.call_args.args[0].proxies, {})
        request = opener.open.call_args.args[0]
        self.assertEqual(request.full_url, "http://192.168.4.1/api/wifi")
        self.assertEqual(urllib.parse.parse_qs(request.data.decode()),
                         {"wifiSsid": ["My Wi-Fi"], "wifiPassword": ["new password"]})

    def test_missing_cloud_configuration_needs_full_setup(self):
        opener = Mock()
        opener.open.return_value = response({"firmwareVersion": "1.6.0", "wifiOnlyAvailable": False})
        with patch("pcbot_linux.wifi_setup.urllib.request.build_opener", return_value=opener):
            with self.assertRaisesRegex(RuntimeError, "full setup"):
                save_wifi("network", "password")
        self.assertEqual(opener.open.call_count, 1)

    def test_older_firmware_does_not_receive_password(self):
        opener = Mock()
        opener.open.return_value = response({"deviceId": "desktop-01"})
        with patch("pcbot_linux.wifi_setup.urllib.request.build_opener", return_value=opener):
            with self.assertRaisesRegex(RuntimeError, "Open setup page"):
                save_wifi("network", "password")
        self.assertEqual(opener.open.call_count, 1)

    def test_invalid_wifi_never_sends_request(self):
        with patch("pcbot_linux.wifi_setup.urllib.request.build_opener") as factory:
            for ssid, password in (("", "password"), ("a" * 33, "password"), ("net", "short")):
                with self.assertRaises(ValueError):
                    save_wifi(ssid, password)
            factory.assert_not_called()

    def test_offline_error_explains_joining_hotspot(self):
        opener = Mock()
        opener.open.side_effect = urllib.error.URLError("offline")
        with patch("pcbot_linux.wifi_setup.urllib.request.build_opener", return_value=opener):
            with self.assertRaisesRegex(RuntimeError, "PICO setup hotspot"):
                save_wifi("network", "password")

    def test_redirect_cannot_forward_wifi_credentials(self):
        with self.assertRaises(RuntimeError):
            NoRedirect().redirect_request(None, None, 302, None, None, "https://example.com")


if __name__ == "__main__":
    unittest.main()

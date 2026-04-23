import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UI_FILE = ROOT / "include" / "web" / "mcp2515_dashboard_ui.h"
DASH_FILE = ROOT / "include" / "web" / "mcp2515_dashboard.h"


class WifiSettingsRegressionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ui = UI_FILE.read_text(encoding="utf-8")
        cls.dash = DASH_FILE.read_text(encoding="utf-8")

    def test_wifi_ui_has_all_expected_fields(self) -> None:
        required_ids = [
            "wifi-status",
            "wifi-ssid",
            "wifi-pass",
            "wifi-static",
            "wifi-ip",
            "wifi-gw",
            "wifi-mask",
            "wifi-dns",
            "wifi-nets",
            "scan-btn",
        ]

        for element_id in required_ids:
            with self.subTest(element_id=element_id):
                self.assertIn(f'id="{element_id}"', self.ui)

    def test_wifi_ui_scripts_reference_existing_elements(self) -> None:
        referenced_ids = {
            match.group(1)
            for match in re.finditer(r"\$\('([^']+)'\)", self.ui)
            if match.group(1).startswith("wifi-")
        }
        declared_ids = {
            match.group(1)
            for match in re.finditer(r'id="([^"]+)"', self.ui)
            if match.group(1).startswith("wifi-")
        }

        missing = sorted(referenced_ids - declared_ids)
        self.assertEqual([], missing, f"Missing WiFi UI ids: {missing}")

    def test_wifi_backend_exposes_routes_and_preference_keys(self) -> None:
        required_routes = ["/wifi_scan", "/wifi_config", "/wifi_status"]
        required_pref_keys = [
            "wifi_ssid",
            "wifi_pass",
            "wifi_static",
            "wifi_ip",
            "wifi_gw",
            "wifi_mask",
            "wifi_dns",
        ]

        for route in required_routes:
            with self.subTest(route=route):
                self.assertIn(route, self.dash)

        for key in required_pref_keys:
            with self.subTest(pref_key=key):
                self.assertIn(f'"{key}"', self.dash)

    def test_wifi_status_payload_covers_connection_and_config_state(self) -> None:
        expected_fields = [
            '"connected"',
            '"ssid"',
            '"stored"',
            '"ip"',
            '"static"',
            '"cfg_ip"',
            '"cfg_gw"',
            '"cfg_mask"',
            '"cfg_dns"',
            '"connecting"',
        ]

        status_section = self.dash[self.dash.index("static void handleWifiStatus()") :]
        for field in expected_fields:
            with self.subTest(field=field):
                self.assertIn(field, status_section)

    def test_wifi_settings_backup_roundtrip_fields_exist(self) -> None:
        expected_export_import_fields = [
            '"wifi":{"ssid"',
            'doc["wifi"].is<JsonObject>()',
            'doc["wifi"]["ssid"]',
            'doc["wifi"]["pass"]',
            'doc["wifi"]["static"]',
            'doc["wifi"]["ip"]',
            'doc["wifi"]["gw"]',
            'doc["wifi"]["mask"]',
            'doc["wifi"]["dns"]',
        ]

        for field in expected_export_import_fields:
            with self.subTest(field=field):
                self.assertIn(field, self.dash)


if __name__ == "__main__":
    unittest.main()

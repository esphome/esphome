"""bluetooth_proxy mirrors esp32_ble.IDF_MAX_CONNECTIONS; pin them together.

The mirror doubles as the outer CONFIG_SCHEMA's connection_slots bound, and
the esp32 schema builder lazily imports esp32_ble and asserts the two values
agree, but that assert only fires while building the esp32 schema. This test
catches drift when the upstream constant changes without any esp32 config
being validated.
"""

from esphome.components import esp32_ble
from esphome.components.bluetooth_proxy import _IDF_MAX_CONNECTIONS


def test_mirror_matches_esp32_ble() -> None:
    assert _IDF_MAX_CONNECTIONS == esp32_ble.IDF_MAX_CONNECTIONS, (
        "bluetooth_proxy._IDF_MAX_CONNECTIONS is out of sync with "
        "esp32_ble.IDF_MAX_CONNECTIONS; update the mirror in "
        "esphome/components/bluetooth_proxy/__init__.py"
    )

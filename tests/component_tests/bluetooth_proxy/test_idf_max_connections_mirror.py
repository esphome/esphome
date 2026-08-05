"""bluetooth_proxy mirrors esp32_ble.IDF_MAX_CONNECTIONS; pin them together.

The mirror exists so the statically walkable CONFIG_SCHEMA can express the
connection_slots range without importing the esp32 BLE stack (that import
registers esp32-only automations on every platform). The runtime check in
_esp32_config_schema() only fires while validating an esp32 config, so this
test is what actually catches drift when the upstream constant changes.
"""

from esphome.components import esp32_ble
from esphome.components.bluetooth_proxy import _IDF_MAX_CONNECTIONS


def test_mirror_matches_esp32_ble() -> None:
    assert _IDF_MAX_CONNECTIONS == esp32_ble.IDF_MAX_CONNECTIONS, (
        "bluetooth_proxy._IDF_MAX_CONNECTIONS is out of sync with "
        "esp32_ble.IDF_MAX_CONNECTIONS; update the mirror in "
        "esphome/components/bluetooth_proxy/__init__.py"
    )

"""Per-platform GATT connection backends the Bluetooth proxy drives.

Auto-loaded by bluetooth_proxy, no user-facing configuration; the proxy's
codegen declares and registers the connection instances.
"""

import functools

import esphome.codegen as cg
from esphome.config_helpers import filter_source_files_from_platform
from esphome.const import PlatformFramework
from esphome.core import CORE


def AUTO_LOAD() -> list[str]:
    """The esp32 connection header includes esp32_ble_client, so the closure
    must be self-satisfying; no target platform (tooling) gets the union."""
    if CORE.is_esp32 or CORE.target_platform is None:
        return ["ble_device_base", "esp32_ble_client"]
    return ["ble_device_base"]


CODEOWNERS = ["@bdraco", "@jesserockz"]

bluetooth_connection_ns = cg.esphome_ns.namespace("bluetooth_connection")


@functools.cache
def esp32_connection_class() -> cg.MockObjClass:
    """Lazy: importing esp32_ble_client registers esp32-only automations as
    an import side effect, which must not leak into other platforms."""
    from esphome.components import esp32_ble_client

    return bluetooth_connection_ns.class_(
        "BluetoothConnection", esp32_ble_client.BLEClientBase
    )


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "bluetooth_connection_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
    }
)

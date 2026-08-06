"""Per-platform GATT connection backends for the Bluetooth proxy.

This component holds the BluetoothConnection implementations the proxy drives
for active connections (esp32 Bluedroid today). It is auto-loaded by
bluetooth_proxy and has no user-facing configuration; the proxy's codegen
declares and registers the connection instances.
"""

import functools

import esphome.codegen as cg
from esphome.config_helpers import filter_source_files_from_platform
from esphome.const import PlatformFramework
from esphome.core import CORE


def AUTO_LOAD() -> list[str]:
    """The esp32 connection header includes esp32_ble_client; make the build
    closure self-satisfying on every platform instead of relying on the
    consumer's auto loads."""
    if CORE.is_esp32:
        return ["ble_device_base", "esp32_ble_client"]
    return ["ble_device_base"]


CODEOWNERS = ["@bdraco", "@jesserockz"]

bluetooth_connection_ns = cg.esphome_ns.namespace("bluetooth_connection")


@functools.cache
def esp32_connection_class() -> cg.MockObjClass:
    """Declare the esp32 BluetoothConnection codegen class.

    Lazy: importing esp32_ble_client pulls in the esp32 BLE stack, whose
    modules register esp32-only automations as an import side effect; they
    must not leak into other platforms' registries (see bluetooth_proxy).
    """
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

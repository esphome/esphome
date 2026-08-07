"""Per-platform GATT connection backends the Bluetooth proxy drives.

Backends: esp32 Bluedroid, rp2 BTstack. Auto-loaded by bluetooth_proxy, no
user-facing configuration; the proxy's codegen declares and registers the
connection instances.
"""

import functools

import esphome.codegen as cg
from esphome.config_helpers import filter_source_files_from_platform
from esphome.const import PLATFORM_RP2, PlatformFramework
from esphome.core import CORE


def AUTO_LOAD() -> list[str]:
    """The esp32 connection header includes esp32_ble_client, so the closure
    must be self-satisfying; no target platform (tooling) gets the union."""
    if CORE.is_esp32 or CORE.target_platform is None:
        return ["ble_device_base", "esp32_ble_client"]
    return ["ble_device_base"]


CODEOWNERS = ["@bdraco", "@jesserockz"]

bluetooth_connection_ns = cg.esphome_ns.namespace("bluetooth_connection")

# arduino-pico's prebuilt BTstack is compiled with MAX_NR_GATT_CLIENTS 1;
# raising this needs an upstream change (the layer itself supports N).
RP2_MAX_CONNECTIONS = 1

# Hub platforms with a GATT backend, mapped to their slot limit — the single
# registry of which hub platforms run the connection-capable proxy.
HUB_MAX_CONNECTIONS: dict[str, int] = {PLATFORM_RP2: RP2_MAX_CONNECTIONS}

# The hub-platform wrapper and the rp2 BTstack backend codegen classes.
HubBluetoothConnection = bluetooth_connection_ns.class_("BluetoothConnection")
RP2GattClient = bluetooth_connection_ns.class_("RP2GattClient", cg.Component)


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
        # Every hub platform the proxy admits (the file compiles empty where
        # USE_BLE_GATT_CLIENT is not defined), so a platform gaining a backend
        # cannot hit a missing-symbol trap here.
        "bluetooth_connection_hub.cpp": {
            PlatformFramework.RP2_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
        "bluetooth_connection_rp2.cpp": {PlatformFramework.RP2_ARDUINO},
    }
)

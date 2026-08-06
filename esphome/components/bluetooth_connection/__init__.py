"""Per-platform GATT connection backends for the Bluetooth proxy.

This component holds the BluetoothConnection implementations the proxy drives
for active connections (esp32 Bluedroid today, rp2 BTstack). It is auto-loaded
by bluetooth_proxy and has no user-facing configuration; the proxy's codegen
declares and registers the connection instances. The connection sources
deliberately include bluetooth_proxy.h: the proxy is the one consumer, and the
API-message emission lives with the connection rather than behind another
indirection.
"""

import functools

import esphome.codegen as cg
from esphome.config_helpers import filter_source_files_from_platform
from esphome.const import PLATFORM_RP2, PlatformFramework
from esphome.core import CORE


def AUTO_LOAD() -> list[str]:
    """The esp32 connection header includes esp32_ble_client; make the build
    closure self-satisfying on every platform instead of relying on the
    consumer's auto loads. With no target platform (tooling resolving the
    manifest), expose the union so dependency closures stay complete."""
    if CORE.is_esp32 or CORE.target_platform is None:
        return ["ble_device_base", "esp32_ble_client"]
    return ["ble_device_base"]


CODEOWNERS = ["@bdraco", "@jesserockz"]

bluetooth_connection_ns = cg.esphome_ns.namespace("bluetooth_connection")

# Connection-slot limit of the rp2 BTstack backend. The prebuilt BTstack
# library in arduino-pico is compiled with MAX_NR_GATT_CLIENTS 1, so exactly
# one concurrent GATT connection exists; raising this requires an upstream
# arduino-pico change. The layer itself is built for N connections.
RP2_MAX_CONNECTIONS = 1

# Hub platforms with a GATT connection backend, mapped to their slot limit —
# the single registry of "which hub platforms run the full proxy".
HUB_MAX_CONNECTIONS: dict[str, int] = {PLATFORM_RP2: RP2_MAX_CONNECTIONS}

# The hub-platform wrapper and the rp2 BTstack backend codegen classes.
HubBluetoothConnection = bluetooth_connection_ns.class_("BluetoothConnection")
RP2GattClient = bluetooth_connection_ns.class_("RP2GattClient", cg.Component)


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

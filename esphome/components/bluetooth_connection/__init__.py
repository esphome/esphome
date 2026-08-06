"""Per-platform GATT connection backends for the Bluetooth proxy.

This component holds the BluetoothConnection implementations the proxy drives
for active connections (esp32 Bluedroid today, rp2 BTstack). It is auto-loaded
by bluetooth_proxy and has no user-facing configuration; the proxy's codegen
declares and registers the connection instances.
"""

import functools

import esphome.codegen as cg
from esphome.config_helpers import filter_source_files_from_platform
from esphome.const import PLATFORM_RP2, PlatformFramework

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
        # Hub platforms with a GATT client backend
        "bluetooth_connection_hub.cpp": {PlatformFramework.RP2_ARDUINO},
        "bluetooth_connection_rp2.cpp": {PlatformFramework.RP2_ARDUINO},
    }
)

"""Per-platform GATT connection backends and the helpers to embed one.

Backends: esp32 Bluedroid, rp2 BTstack. No user-facing configuration; a
consumer's codegen declares and registers the backend instances — the
Bluetooth proxy through its per-slot connection wrappers, and components
owning a dedicated backend (e.g. radon_eye_rd200) through
gatt_client_schema() + new_gatt_backend().
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.config_helpers import filter_source_files_from_platform
from esphome.const import PLATFORM_RP2, PlatformFramework
from esphome.core import CORE
from esphome.types import ConfigType


def AUTO_LOAD() -> list[str]:
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
BluedroidGattClient = bluetooth_connection_ns.class_(
    "BluedroidGattClient", cg.Component
)

CONF_BACKEND_ID = "backend_id"


def gatt_client_schema() -> cv.Schema:
    """Schema fragment for one GATT backend instance: its generated id plus
    the platform-stack reference new_gatt_backend() resolves. Platform
    dispatch happens at call time, so call this from inside a validator or a
    per-platform schema builder, never at module import.
    """
    if CORE.is_esp32:
        from esphome.components import esp32_ble_tracker

        return esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA.extend(
            {cv.GenerateID(CONF_BACKEND_ID): cv.declare_id(BluedroidGattClient)}
        )
    from esphome.components import rp2040_ble

    return cv.Schema(
        {
            cv.GenerateID(CONF_BACKEND_ID): cv.declare_id(RP2GattClient),
            cv.GenerateID(rp2040_ble.CONF_RP2040_BLE_ID): cv.use_id(
                rp2040_ble.RP2040BLE
            ),
        }
    )


async def new_gatt_backend(config: ConfigType) -> cg.MockObj:
    """Instantiate the backend declared by gatt_client_schema(), register it
    with its platform stack, and claim one neutral GATT client slot.

    On esp32 the tracker's promote loop owns connect timing, so the backend's
    tracker-facing shim registers as a raw client; on rp2 the backend parents
    on the BTstack controller.
    """
    from esphome.components import ble_device_base

    ble_device_base.request_gatt_client()
    backend = cg.new_Pvariable(config[CONF_BACKEND_ID])
    await cg.register_component(backend, config)
    if CORE.is_esp32:
        from esphome.components import esp32_ble_tracker

        await esp32_ble_tracker.register_raw_client(backend.tracker_client(), config)
    else:
        from esphome.components import rp2040_ble

        await cg.register_parented(backend, config[rp2040_ble.CONF_RP2040_BLE_ID])
    return backend


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "bluetooth_connection_bluedroid.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
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
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "bluetooth_connection_rp2.cpp": {PlatformFramework.RP2_ARDUINO},
    }
)

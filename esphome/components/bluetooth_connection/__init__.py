"""Per-platform GATT connection backends and the helpers to embed one.

Backends: esp32 Bluedroid, rp2 BTstack. No user-facing configuration; the
Bluetooth proxy's codegen declares and registers the backend instances
through gatt_client_schema()/hub_connection_schema() + new_gatt_backend().
"""

from collections.abc import Awaitable, Callable
from dataclasses import dataclass

import esphome.codegen as cg
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import PLATFORM_ESP32, PLATFORM_RP2, PlatformFramework
from esphome.core import CORE
from esphome.types import ConfigType

DOMAIN = "bluetooth_connection"


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

# The hub-platform wrapper and the backend codegen classes.
HubBluetoothConnection = bluetooth_connection_ns.class_("BluetoothConnection")
RP2GattClient = bluetooth_connection_ns.class_("RP2GattClient", cg.Component)
BluedroidGattClient = bluetooth_connection_ns.class_(
    "BluedroidGattClient", cg.Component
)

CONF_BACKEND_ID = "backend_id"


def _esp32_schema_fragment() -> cv.Schema:
    from esphome.components import esp32_ble_tracker

    return esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA


def _rp2_schema_fragment() -> cv.Schema:
    from esphome.components import rp2040_ble

    return cv.Schema(
        {cv.GenerateID(rp2040_ble.CONF_RP2040_BLE_ID): cv.use_id(rp2040_ble.RP2040BLE)}
    )


async def _esp32_register(backend: cg.MockObj, config: ConfigType) -> None:
    from esphome.components import esp32_ble_tracker

    # The tracker's promote loop owns connect timing; the backend's
    # tracker-facing shim registers as a raw client.
    await esp32_ble_tracker.register_raw_client(backend.tracker_client(), config)


async def _rp2_register(backend: cg.MockObj, config: ConfigType) -> None:
    from esphome.components import rp2040_ble

    await cg.register_parented(backend, config[rp2040_ble.CONF_RP2040_BLE_ID])


@dataclass(frozen=True)
class _PlatformBackend:
    """One platform's backend: codegen class, extra schema keys (lazy so the
    platform stack is only imported when targeted), and stack registration."""

    backend_class: cg.MockObjClass
    schema_fragment: Callable[[], cv.Schema]
    register: Callable[[cg.MockObj, ConfigType], Awaitable[None]]


# The single registry of platforms with a GATT client backend; a platform
# missing here fails loudly everywhere instead of falling into another
# platform's arm.
_PLATFORM_BACKENDS: dict[str, _PlatformBackend] = {
    PLATFORM_ESP32: _PlatformBackend(
        BluedroidGattClient, _esp32_schema_fragment, _esp32_register
    ),
    PLATFORM_RP2: _PlatformBackend(RP2GattClient, _rp2_schema_fragment, _rp2_register),
}


def _backend_entry(platform: str | None = None) -> _PlatformBackend:
    key = platform if platform is not None else CORE.target_platform
    if (entry := _PLATFORM_BACKENDS.get(key)) is None:
        raise cv.Invalid(f"no GATT client backend is registered for {key}")
    return entry


def gatt_client_schema(platform: str | None = None) -> cv.Schema:
    """Schema fragment for one GATT backend instance: its generated id plus
    the platform-stack reference new_gatt_backend() resolves.

    Defaults to the platform being validated; pass `platform` explicitly when
    building a schema outside validation (the language-schema dumper calls
    per-platform builders under arbitrary CORE platforms).
    """
    entry = _backend_entry(platform)
    return entry.schema_fragment().extend(
        {cv.GenerateID(CONF_BACKEND_ID): cv.declare_id(entry.backend_class)}
    )


def hub_connection_schema(platform: str | None = None) -> cv.Schema:
    """Per-slot schema for the proxy's connection wrappers: the wrapper id on
    top of the backend fragment. Same platform rules as gatt_client_schema()."""
    return gatt_client_schema(platform).extend(
        {cv.GenerateID(): cv.declare_id(HubBluetoothConnection)}
    )


async def new_gatt_backend(config: ConfigType) -> cg.MockObj:
    """Instantiate the backend declared by gatt_client_schema() and register
    it with its platform stack. The connection slot is claimed at validation
    (the proxy's slot validators), not here.
    """
    from esphome.components import ble_device_base

    ble_device_base.request_gatt_client()
    backend = cg.new_Pvariable(config[CONF_BACKEND_ID])
    # The backend has no user-facing component options; an empty config keeps
    # the consumer's own keys (update_interval, ...) off it.
    await cg.register_component(backend, {})
    await _backend_entry().register(backend, config)
    return backend


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "bluetooth_connection_bluedroid.cpp": {
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

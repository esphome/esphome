"""Per-platform GATT connection backends and the helpers to embed one.

Backends: esp32 Bluedroid, rp2 BTstack. No user-facing configuration; the
Bluetooth proxy's codegen declares and registers the backend instances
through gatt_client_schema()/hub_connection_schema() + new_gatt_backend().
"""

from collections.abc import Awaitable, Callable
from dataclasses import dataclass

import esphome.codegen as cg
from esphome.components import rp2040_ble
from esphome.config_helpers import (
    filter_source_files_from_platform,
    frameworks_for_platforms,
)
import esphome.config_validation as cv
from esphome.const import PLATFORM_ESP32, PLATFORM_RP2, PlatformFramework
from esphome.core import CORE
from esphome.types import ConfigType


def AUTO_LOAD() -> list[str]:
    """ble_device_base plus the platform BLE stack the build's backend
    registers with (the Bluedroid header includes the tracker's), so
    consumers need not know. The platform-less arm serves manifest tooling."""
    if CORE.is_esp32:
        return ["ble_device_base", "esp32_ble_tracker"]
    if CORE.is_rp2:
        return ["ble_device_base", "rp2040_ble"]
    if CORE.target_platform is None:
        return ["ble_device_base", "esp32_ble_tracker", "rp2040_ble"]
    return ["ble_device_base"]


CODEOWNERS = ["@bdraco", "@jesserockz"]

bluetooth_connection_ns = cg.esphome_ns.namespace("bluetooth_connection")

# arduino-pico's prebuilt BTstack is compiled with MAX_NR_GATT_CLIENTS 1 and
# MAX_NR_HCI_CONNECTIONS 2; for more than one backend, rp2040_ble's
# btstack_memory.cpp replaces those pools via linker --wrap (requested by
# _rp2_register), sized from ESPHOME_BLE_GATT_CLIENT_COUNT. The cap itself
# belongs to the platform stack that owns the pools.
RP2_MAX_CONNECTIONS = rp2040_ble.MAX_CONNECTIONS

# Slot limits for the hub platforms running the connection-capable proxy;
# the backend registry itself is _PLATFORM_BACKENDS below.
HUB_MAX_CONNECTIONS: dict[str, int] = {PLATFORM_RP2: RP2_MAX_CONNECTIONS}

# The hub-platform wrapper and the backend codegen classes.
HubBluetoothConnection = bluetooth_connection_ns.class_("BluetoothConnection")
RP2GattClient = bluetooth_connection_ns.class_("RP2GattClient", cg.Component)
BluedroidGattClient = bluetooth_connection_ns.class_(
    "BluedroidGattClient", cg.Component
)

CONF_BACKEND_ID = "backend_id"

DOMAIN = "bluetooth_connection"


@dataclass
class _ConnectionData:
    rp2_backend_count: int = 0


def _get_data() -> _ConnectionData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = _ConnectionData()
    return CORE.data[DOMAIN]


def _esp32_schema_fragment() -> cv.Schema:
    from esphome.components import esp32_ble_tracker

    return esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA


def _rp2_schema_fragment() -> cv.Schema:
    return cv.Schema(
        {cv.GenerateID(rp2040_ble.CONF_RP2040_BLE_ID): cv.use_id(rp2040_ble.RP2040BLE)}
    )


async def _esp32_register(backend: cg.MockObj, config: ConfigType) -> None:
    from esphome.components import esp32_ble_tracker

    # The tracker's promote loop owns connect timing; the backend registers
    # as a raw client (it is the tracker's ESPBTClient).
    await esp32_ble_tracker.register_raw_client(backend, config)


async def _rp2_register(backend: cg.MockObj, config: ConfigType) -> None:
    from esphome.components import ota

    # The backend drops its link when an OTA starts (esp32 tracker parity).
    ota.request_ota_state_listeners()
    # More than one backend outgrows the prebuilt BTstack pools: swap them for
    # the ESPHOME_BLE_GATT_CLIENT_COUNT-sized ones in rp2040_ble's
    # btstack_memory.cpp. Keyed to backend registrations (the same event that
    # grows the count that sizes the pools), so single-backend builds emit no
    # flags and stay byte-identical to previous releases.
    data = _get_data()
    data.rp2_backend_count += 1
    if data.rp2_backend_count == 2:
        rp2040_ble.add_btstack_pool_overrides()
    await cg.register_parented(backend, config[rp2040_ble.CONF_RP2040_BLE_ID])


@dataclass(frozen=True)
class _PlatformBackend:
    """One platform's backend: codegen class, extra schema keys, and stack
    registration. The esp32 fragments import their stack lazily because those
    imports register esp32-only automations as a side effect; rp2040_ble is
    side-effect-free, so it is imported at module scope (the cap constant
    needs it there anyway)."""

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
    top of the backend fragment, plus the component keys (setup_priority and
    friends now apply to the backend, the slot's real Component). Same
    platform rules as gatt_client_schema()."""
    return (
        gatt_client_schema(platform)
        .extend({cv.GenerateID(): cv.declare_id(HubBluetoothConnection)})
        .extend(cv.COMPONENT_SCHEMA)
    )


async def new_gatt_backend(config: ConfigType) -> cg.MockObj:
    """Instantiate the backend declared by gatt_client_schema() and register
    it with its platform stack. The connection slot is claimed at validation
    (the proxy's slot validators), not here.
    """
    from esphome.components import ble_device_base

    ble_device_base.request_gatt_client()
    backend = cg.new_Pvariable(config[CONF_BACKEND_ID])
    # The backend is the slot's real Component: component keys from the
    # connection entry (setup_priority, ...) apply to it. Consumers whose own
    # schema carries keys that register_component would misapply to the
    # backend (e.g. a polling interval) must not put them in this config.
    await cg.register_component(backend, config)
    await _backend_entry().register(backend, config)
    return backend


# Named so tests can pin the hub entry against bluetooth_proxy's platform
# list (this module cannot import bluetooth_proxy to derive it).
SOURCE_FILE_FRAMEWORKS: dict[str, set[PlatformFramework]] = {
    "bluetooth_connection_bluedroid.cpp": frameworks_for_platforms([PLATFORM_ESP32]),
    # Every hub platform the proxy admits (the file compiles empty where
    # USE_BLE_GATT_CLIENT is not defined), so a platform gaining a backend
    # cannot hit a missing-symbol trap here.
    "bluetooth_connection_hub.cpp": {
        PlatformFramework.RP2_ARDUINO,
        PlatformFramework.LN882X_ARDUINO,
        PlatformFramework.BK72XX_ARDUINO,
        PlatformFramework.ESP32_ARDUINO,
        PlatformFramework.ESP32_IDF,
    },
    "bluetooth_connection_rp2.cpp": {PlatformFramework.RP2_ARDUINO},
}

FILTER_SOURCE_FILES = filter_source_files_from_platform(SOURCE_FILE_FRAMEWORKS)

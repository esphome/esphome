"""Per-platform GATT connection backends and the helpers to embed one.

Backends: esp32 Bluedroid, rp2 BTstack. No user-facing configuration; a
consumer's codegen declares and registers the backend instances — the
Bluetooth proxy through its per-slot connection wrappers (a streaming
consumer), and direct consumers owning a dedicated backend through
gatt_client_config_schema() + new_gatt_backend().
"""

from collections.abc import Awaitable, Callable
from dataclasses import dataclass, field

import esphome.codegen as cg
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAC_ADDRESS,
    PLATFORM_ESP32,
    PLATFORM_RP2,
    PlatformFramework,
)
from esphome.core import CORE
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

DOMAIN = "bluetooth_connection"


def AUTO_LOAD() -> list[str]:
    """ble_device_base plus the platform BLE stack the build's backend
    registers with, so consumers stay platform-blind. The platform-less arm
    serves tooling that resolves the manifest without a target."""
    if CORE.is_esp32:
        return ["ble_device_base", "esp32_ble_tracker"]
    if CORE.is_rp2:
        return ["ble_device_base", "rp2040_ble"]
    if CORE.target_platform is None:
        return ["ble_device_base", "esp32_ble_tracker", "rp2040_ble"]
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
CONF_ADDRESS_TYPE = "address_type"

# BLE_ADDR_TYPE_* code space shared with the API and the backends.
ADDRESS_TYPES = {"public": 0, "random": 1}


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

# Gates dedicated-backend consumers (cv.only_on).
GATT_CLIENT_PLATFORMS = list(_PLATFORM_BACKENDS)


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


@dataclass
class _SlotLedger:
    """GATT connection slots claimed this run, for the platform cap check."""

    consumers: list[str] = field(default_factory=list)


def _ledger() -> _SlotLedger:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = _SlotLedger()
    return CORE.data[DOMAIN]


def consume_gatt_slot(consumer: str, count: int = 1):
    """Validator claiming GATT connection slots — the one spelling for every
    claimant (the proxy per configured slot, dedicated backends once). The
    neutral ledger feeds the platform cap check in FINAL_VALIDATE_SCHEMA;
    esp32 additionally charges the controller's connection budget."""

    def validator(config: ConfigType) -> ConfigType:
        _ledger().consumers.extend([consumer] * count)
        if CORE.is_esp32:
            from esphome.components import esp32_ble

            esp32_ble.consume_connection_slots(count, consumer)(config)
        return config

    return validator


def _validate_slot_totals(config: ConfigType) -> ConfigType:
    # esp32 has its own controller budget (esp32_ble); the hub platforms cap
    # at the prebuilt stack's client count, and nothing else counts claims
    # across components (e.g. a proxy plus a radon_eye_rd200 on rp2).
    if (cap := HUB_MAX_CONNECTIONS.get(CORE.target_platform)) is None:
        return config
    claimed = _ledger().consumers
    if len(claimed) > cap:
        raise cv.Invalid(
            f"{CORE.target_platform} supports at most {cap} GATT client "
            f"connection(s); {len(claimed)} requested by: {', '.join(claimed)}"
        )
    return config


FINAL_VALIDATE_SCHEMA = _validate_slot_totals


# The peer keys every dedicated-backend consumer shares: one target device.
_PEER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Optional(CONF_ADDRESS_TYPE, default="public"): cv.enum(
            ADDRESS_TYPES, lower=True
        ),
    }
)


def gatt_client_config_schema(base_schema: cv.Schema, consumer: str):
    """Wrap a dedicated-backend consumer's schema so the consumer stays
    platform-blind: gates on the platforms with a backend, folds in
    gatt_client_schema() plus the peer keys (mac_address, address_type),
    and claims the connection slot. `consumer` names the component in
    slot-exhaustion errors."""

    @schema_extractor("schema")
    def apply(config: ConfigType) -> ConfigType:
        if config is SCHEMA_EXTRACT:
            # The language-schema dumper runs without a platform; expose the
            # consumer's keys plus the platform-free peer keys.
            return base_schema.extend(_PEER_SCHEMA)
        cv.only_on(GATT_CLIENT_PLATFORMS)(config)
        schema = base_schema.extend(_PEER_SCHEMA).extend(gatt_client_schema())
        config = schema(config)
        return consume_gatt_slot(consumer)(config)

    return apply


async def new_gatt_backend(
    config: ConfigType, *, service_table: bool = True
) -> cg.MockObj:
    """Instantiate the backend declared by gatt_client_schema() and register
    it with its platform stack. The connection slot is claimed at validation
    (gatt_client_config_schema / the proxy's slot validators), not here.

    service_table compiles the on-demand service-table materializer into the
    backend; direct consumers need it, the streaming proxy does not, so
    proxy-only builds keep the smaller footprint.
    """
    from esphome.components import ble_device_base

    ble_device_base.request_gatt_client()
    if service_table:
        cg.add_define("USE_BLE_GATT_SERVICE_TABLE")
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

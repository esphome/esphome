"""Per-platform GATT connection backends and the helpers to embed one.

Backends: esp32 Bluedroid, rp2 BTstack. No user-facing configuration; a
consumer's codegen declares and registers the backend instances — the
Bluetooth proxy through its per-slot connection wrappers, and components
owning a dedicated backend (e.g. radon_eye_rd200) through
gatt_client_schema() + new_gatt_backend().
"""

from dataclasses import dataclass, field

import esphome.codegen as cg
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import PLATFORM_ESP32, PLATFORM_RP2, PlatformFramework
from esphome.core import CORE
from esphome.schema_extractors import SCHEMA_EXTRACT
from esphome.types import ConfigType

DOMAIN = "bluetooth_connection"


def AUTO_LOAD() -> list[str]:
    """ble_device_base plus the platform BLE stack the build's backend
    registers with, so consumers stay platform-blind. The platform-less arm
    serves tooling that resolves the manifest without a target."""
    if CORE.is_esp32:
        return ["ble_device_base", "esp32_ble_tracker"]
    if CORE.target_platform == PLATFORM_RP2:
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

# Every platform with a GATT backend; gates dedicated-backend consumers.
# Derived from the hub registry so a platform gaining a backend is admitted
# everywhere at once (esp32 is the non-hub arm).
GATT_CLIENT_PLATFORMS = [PLATFORM_ESP32, *HUB_MAX_CONNECTIONS]

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


def hub_connection_schema() -> cv.Schema:
    """Per-slot schema for the proxy's connection wrappers: the wrapper id on
    top of the backend fragment. Same call-time constraint as
    gatt_client_schema()."""
    return gatt_client_schema().extend(
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


def consume_gatt_slot(consumer: str):
    """Validator claiming one GATT connection slot: the neutral ledger feeds
    the platform cap check in FINAL_VALIDATE_SCHEMA, and esp32 additionally
    charges the controller's connection budget."""

    def validator(config: ConfigType) -> ConfigType:
        _ledger().consumers.append(consumer)
        if CORE.is_esp32:
            from esphome.components import esp32_ble

            esp32_ble.consume_connection_slots(1, consumer)(config)
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


def gatt_client_config_schema(base_schema: cv.Schema, consumer: str):
    """Wrap a dedicated-backend consumer's schema so the consumer stays
    platform-blind: gates on the platforms with a backend, folds in
    gatt_client_schema(), and claims the connection slot.
    `consumer` names the component in slot-exhaustion errors."""

    def apply(config: ConfigType) -> ConfigType:
        if config is SCHEMA_EXTRACT:
            # The language-schema dumper runs without a platform; expose the
            # consumer's own keys. Checked before the platform gate so the
            # dumper is not rejected by only_on.
            return base_schema
        cv.only_on(GATT_CLIENT_PLATFORMS)(config)
        config = base_schema.extend(gatt_client_schema())(config)
        return consume_gatt_slot(consumer)(config)

    return apply


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
    # The backend has no user-facing component options; an empty config keeps
    # the consumer's own keys (update_interval, ...) off it.
    await cg.register_component(backend, {})
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

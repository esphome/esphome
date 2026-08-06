import functools
import logging

import esphome.codegen as cg
from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import CONF_ACTIVE, CONF_ID, PLATFORM_LN882X, PLATFORM_RP2
from esphome.core import CORE
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

# The esp32 BLE stack (esp32_ble, esp32_ble_client, esp32_ble_tracker) is
# imported lazily inside _esp32_config_schema()/_to_code_esp32(): importing
# those modules registers esp32-only automations (ble.enable, ble.disable, ...)
# as a side effect, and a module-scope import would leak them into every
# platform's registry the moment a config declares `bluetooth_proxy:` —
# degrading "Unable to find action" config errors into C++ compile failures.


def AUTO_LOAD(config: ConfigType | None = None) -> list[str]:
    """Components to auto-load for the platform being compiled.

    Callable with no argument so tooling that resolves AUTO_LOAD without a
    target platform (the device-builder catalog sync does exactly this) gets
    the union of every arm instead of an empty list — which is what lets it
    keep cross-referencing the esp32 BLE stack. A real build always has a
    target platform set, so it takes one of the concrete branches.
    """
    if CORE.is_esp32:
        return ["esp32_ble_client", "esp32_ble_tracker"]
    if CORE.target_platform in _HUB_PLATFORMS:
        return ["ble_device_base"]
    # No target platform, or one this component does not support: tooling
    # resolving the manifest (including the host-pinned dependency resolver) —
    # expose every arm so the closure keeps the esp32 BLE stack.
    return ["ble_device_base", "esp32_ble_client", "esp32_ble_tracker"]


# Platforms with an in-tree ble_device_base BLE tracker hub whose controller
# supports active scanning. Passive-only hubs (bk72xx) are deliberately NOT
# admitted yet: every current client (aioesphomeapi, bleak-esphome, Home
# Assistant) assumes an ESPHome proxy can scan actively, so a passive-only
# proxy would be misdriven — bk72xx follows once the API carries a feature
# flag clients can trust (FEATURE_ACTIVE_SCAN + a version flag, separate PRs).
_HUB_PLATFORMS = (PLATFORM_LN882X, PLATFORM_RP2)

DEPENDENCIES = ["api"]
CODEOWNERS = ["@jesserockz", "@bdraco"]

_LOGGER = logging.getLogger(__name__)

CONF_CONNECTION_SLOTS = "connection_slots"
CONF_CACHE_SERVICES = "cache_services"
CONF_CONNECTIONS = "connections"
DEFAULT_CONNECTION_SLOTS = 3

bluetooth_proxy_ns = cg.esphome_ns.namespace("bluetooth_proxy")

BluetoothProxy = bluetooth_proxy_ns.class_("BluetoothProxy", cg.Component)

# Mirrors esp32_ble.IDF_MAX_CONNECTIONS as a literal so the statically walkable
# CONFIG_SCHEMA below can state the connection_slots range without importing the
# esp32 BLE stack. tests/component_tests/bluetooth_proxy/ pins the two together.
_IDF_MAX_CONNECTIONS = 9


@functools.cache
def _esp32_config_schema() -> cv.All:
    """Build the esp32 schema, importing the esp32 BLE stack only when used."""
    from esphome.components import esp32_ble, esp32_ble_client, esp32_ble_tracker

    if esp32_ble.IDF_MAX_CONNECTIONS != _IDF_MAX_CONNECTIONS:
        raise cv.Invalid(
            f"bluetooth_proxy's connection-slot limit mirror "
            f"({_IDF_MAX_CONNECTIONS}) is out of sync with "
            f"esp32_ble.IDF_MAX_CONNECTIONS ({esp32_ble.IDF_MAX_CONNECTIONS}); "
            f"update _IDF_MAX_CONNECTIONS in bluetooth_proxy/__init__.py"
        )

    BluetoothConnection = bluetooth_proxy_ns.class_(
        "BluetoothConnection", esp32_ble_client.BLEClientBase
    )
    CONNECTION_SCHEMA = esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(BluetoothConnection),
        }
    ).extend(cv.COMPONENT_SCHEMA)

    def validate_connections(config):
        if CONF_CONNECTIONS in config:
            if not config[CONF_ACTIVE]:
                raise cv.Invalid(
                    "Connections can only be used if the proxy is set to active"
                )
        elif config[CONF_ACTIVE]:
            connection_slots: int = config[CONF_CONNECTION_SLOTS]
            esp32_ble.consume_connection_slots(connection_slots, "bluetooth_proxy")(
                config
            )

            return {
                **config,
                CONF_CONNECTIONS: [
                    CONNECTION_SCHEMA({}) for _ in range(connection_slots)
                ],
            }
        return config

    return cv.All(
        (
            cv.Schema(
                {
                    **_COMMON_SCHEMA_KEYS,
                    cv.Optional(CONF_ACTIVE, default=True): cv.boolean,
                    cv.Optional(CONF_CACHE_SERVICES, default=True): cv.boolean,
                    cv.Optional(
                        CONF_CONNECTION_SLOTS,
                        default=DEFAULT_CONNECTION_SLOTS,
                    ): cv.All(
                        cv.positive_int,
                        cv.Range(min=1, max=esp32_ble.IDF_MAX_CONNECTIONS),
                    ),
                    cv.Optional(CONF_CONNECTIONS): cv.All(
                        cv.ensure_list(CONNECTION_SCHEMA),
                        cv.Length(min=1, max=esp32_ble.IDF_MAX_CONNECTIONS),
                    ),
                }
            )
            .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
            .extend(cv.COMPONENT_SCHEMA)
        ),
        validate_connections,
    )


def _validate_no_active(config: ConfigType) -> ConfigType:
    if config[CONF_ACTIVE]:
        raise cv.Invalid(
            "Active connections are not supported on this platform; the proxy "
            "forwards advertisements only (set active: false)"
        )
    return config


# Advertisement-only proxy on a neutral BLE hub: the hub's raw-advertisement
# callback feeds the same API batching. GATT/active connections are excluded at
# compile time — only the esp32 build compiles the connection stack; nothing
# reads HubCapabilities::gatt at runtime for this today.
# Keys both platform schemas must declare identically; each arm spreads this
# dict so the shared surface cannot drift. CONF_ACTIVE deliberately stays
# per-arm: its default differs (esp32 True, hub arms False — no GATT).
_COMMON_SCHEMA_KEYS = {
    cv.GenerateID(): cv.declare_id(BluetoothProxy),
}

_BLE_HUB_CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            **_COMMON_SCHEMA_KEYS,
            # Declared directly (BLE_DEVICE_SCHEMA-style): appending a validator
            # after a strict schema rejects an explicit `ble_hub_id` before it
            # runs, and that key is the documented way to disambiguate once a
            # platform has two trackers.
            cv.GenerateID(ble_device_base.CONF_BLE_HUB_ID): cv.use_id(
                ble_device_base.BLEHub
            ),
            cv.Optional(CONF_ACTIVE, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_no_active,
)


@schema_extractor("schema")
def _validate_platform(config: ConfigType) -> ConfigType:
    """Apply the schema for the platform actually being compiled.

    esp32 keeps the full GATT proxy; every other platform gets the
    advertisement-only shape, which rejects the connection-oriented options
    above because its schema does not define them.
    """
    if config is SCHEMA_EXTRACT:
        # The language-schema dumper runs without a platform. Expose the esp32
        # shape so `connections`, the ids and every default stay in the
        # generated schema the editor and dashboard consume.
        return _esp32_config_schema()
    if CORE.is_esp32:
        return _esp32_config_schema()(config)
    if CORE.target_platform not in _HUB_PLATFORMS:
        # Fail here with the actual reason. Without this gate the error surfaces
        # later as an unresolvable hub ID ("Are you missing a hub declaration?")
        # on platforms where no hub component can be declared.
        raise cv.Invalid(
            f"bluetooth_proxy is not supported on {CORE.target_platform}: no "
            "active-scan-capable BLE tracker hub is available for this "
            "platform. It runs on esp32 (full proxy), and the ln882x and rp2 "
            "families (advertisement-only)."
        )
    return _BLE_HUB_CONFIG_SCHEMA(config)


def _reject_connection_keys_off_esp32(config: ConfigType) -> ConfigType:
    """Reject connection-oriented options by name on hub-only platforms.

    Runs before the walkable schema below so the user gets "this option does
    not exist here" instead of the option's esp32 value range (which would
    imply a smaller number is accepted).
    """
    if not isinstance(config, dict) or CORE.is_esp32 or CORE.target_platform is None:
        return config
    if CORE.target_platform not in _HUB_PLATFORMS:
        # No proxy of any kind exists here: fall through so _validate_platform
        # reports "not supported on {platform}" instead of a key-level message
        # implying an advertisement-only proxy is available.
        return config
    for key in (CONF_CONNECTION_SLOTS, CONF_CACHE_SERVICES, CONF_CONNECTIONS):
        if key in config:
            raise cv.Invalid(
                f"'{key}' requires active connection support, which needs the "
                "esp32 GATT stack; this platform runs the advertisement-only "
                "proxy and has no such option",
                path=[key],
            )
    return config


# CONFIG_SCHEMA stays a statically walkable schema: tooling (the dashboard's
# field-range extractor among others) introspects it to discover options and
# their bounds, which a bare dispatch function would hide. It carries the scalar
# keys with no defaults; _validate_platform then runs the real per-platform
# schema, which applies the defaults and rejects options the platform does not
# support.
#
# It deliberately does NOT declare `connections`: this outer schema runs before
# the per-platform one, so any key it transforms is transformed twice. Running
# CONNECTION_SCHEMA twice re-validates an already-generated ID through
# declare_id(), which (unlike use_id) has no guard for an ID instance and
# rejects it as empty. extra=ALLOW_EXTRA passes `connections` through untouched
# for _ESP32_CONFIG_SCHEMA to validate exactly once.
CONFIG_SCHEMA = cv.All(
    _reject_connection_keys_off_esp32,
    cv.Schema(
        {
            cv.Optional(CONF_ACTIVE): cv.boolean,
            cv.Optional(CONF_CACHE_SERVICES): cv.boolean,
            cv.Optional(CONF_CONNECTION_SLOTS): cv.All(
                cv.positive_int,
                cv.Range(min=1, max=_IDF_MAX_CONNECTIONS),
            ),
        },
        extra=cv.ALLOW_EXTRA,
    ),
    _validate_platform,
)


async def _to_code_esp32(config: ConfigType) -> None:
    from esphome.components import esp32_ble, esp32_ble_tracker
    from esphome.components.esp32 import add_idf_sdkconfig_option
    from esphome.components.esp32_ble import BTLoggers

    # Register the loggers this component needs
    esp32_ble.register_bt_logger(BTLoggers.GATT, BTLoggers.L2CAP, BTLoggers.SMP)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_active(config[CONF_ACTIVE]))
    await esp32_ble_tracker.register_raw_ble_device(var, config)
    await esp32_ble_tracker.register_scanner_state_listener(var, config)

    # Define max connections for protobuf fixed array
    connection_count = len(config.get(CONF_CONNECTIONS, []))
    cg.add_define("BLUETOOTH_PROXY_MAX_CONNECTIONS", connection_count)

    for connection_conf in config.get(CONF_CONNECTIONS, []):
        connection_var = cg.new_Pvariable(connection_conf[CONF_ID])
        await cg.register_component(connection_var, connection_conf)
        cg.add(var.register_connection(connection_var))
        await esp32_ble_tracker.register_raw_client(connection_var, connection_conf)

    if config.get(CONF_CACHE_SERVICES):
        add_idf_sdkconfig_option("CONFIG_BT_GATTC_CACHE_NVS_FLASH", True)


async def _to_code_ble_hub(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_active(config[CONF_ACTIVE]))
    hub = await cg.get_variable(config[ble_device_base.CONF_BLE_HUB_ID])
    cg.add(var.set_ble_hub(hub))

    # The api component sizes BluetoothConnectionsFreeResponse.allocated with
    # this define whenever a proxy is present; no connections off-esp32.
    cg.add_define("BLUETOOTH_PROXY_MAX_CONNECTIONS", 0)


async def to_code(config: ConfigType) -> None:
    if CORE.is_esp32:
        await _to_code_esp32(config)
    else:
        await _to_code_ble_hub(config)

    # Define batch size for BLE advertisements
    # Each advertisement is up to 80 bytes when packaged (including protocol overhead)
    # 16 advertisements × 80 bytes (worst case) = 1280 bytes out of ~1320 bytes usable payload
    # This achieves ~97% WiFi MTU utilization while staying under the limit
    cg.add_define("BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE", 16)

    cg.add_define("USE_BLUETOOTH_PROXY")

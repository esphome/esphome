import functools
import logging

import esphome.codegen as cg
from esphome.components import ble_device_base, bluetooth_connection
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTIVE,
    CONF_ID,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_LN882X,
    PLATFORM_RP2,
)
from esphome.core import CORE
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

# The esp32 BLE stack (esp32_ble, esp32_ble_tracker) is imported lazily
# inside _esp32_config_schema()/_to_code_esp32(): importing those modules
# registers esp32-only automations (ble.enable, ble.disable, ...) as a side
# effect, and a module-scope import would leak them into every platform's
# registry the moment a config declares `bluetooth_proxy:` — degrading
# "Unable to find action" config errors into C++ compile failures.


def AUTO_LOAD(config: ConfigType | None = None) -> list[str]:
    """Components to auto-load for the platform being compiled.

    Callable with no argument so tooling that resolves AUTO_LOAD without a
    target platform (the device-builder catalog sync does exactly this) gets
    the union of every arm instead of an empty list — which is what lets it
    keep cross-referencing the esp32 BLE stack. A real build always has a
    target platform set, so it takes one of the concrete branches.
    """
    if CORE.is_esp32:
        return ["bluetooth_connection", "esp32_ble_tracker"]
    if CORE.target_platform in _HUB_PLATFORMS:
        return ["ble_device_base", "bluetooth_connection"]
    # No target platform, or one this component does not support: tooling
    # resolving the manifest (including the host-pinned dependency resolver) —
    # expose every arm so the closure keeps the esp32 BLE stack.
    return [
        "ble_device_base",
        "bluetooth_connection",
        "esp32_ble_tracker",
    ]


# Platforms with an in-tree ble_device_base BLE tracker hub whose controller
# supports active scanning — every current client (aioesphomeapi, bleak-esphome,
# Home Assistant) assumes an ESPHome proxy can scan actively, so a passive-only
# hub must not be admitted (it would be misdriven).
# Coupled to bluetooth_connection: platforms with a GATT backend are also
# listed in its _PLATFORM_BACKENDS registry, HUB_MAX_CONNECTIONS, and
# FILTER_SOURCE_FILES hub entry.
_HUB_PLATFORMS = (PLATFORM_BK72XX, PLATFORM_LN882X, PLATFORM_RP2)

DEPENDENCIES = ["api"]
CODEOWNERS = ["@jesserockz", "@bdraco"]

_LOGGER = logging.getLogger(__name__)

CONF_CONNECTION_SLOTS = "connection_slots"
CONF_CACHE_SERVICES = "cache_services"
CONF_CONNECTIONS = "connections"
DEFAULT_CONNECTION_SLOTS = 3

bluetooth_proxy_ns = cg.esphome_ns.namespace("bluetooth_proxy")

BluetoothProxy = bluetooth_proxy_ns.class_("BluetoothProxy", cg.Component)

# Mirrors esp32_ble.IDF_MAX_CONNECTIONS (the loosest platform cap): the esp32
# schema builder asserts the two agree, tests/component_tests/bluetooth_proxy/
# pins them together, and the outer walkable schema uses it as the
# connection_slots bound (per-platform schemas tighten it).
_IDF_MAX_CONNECTIONS = 9


@functools.cache
def _esp32_config_schema() -> cv.All:
    """Build the esp32 schema, importing the esp32 BLE stack only when used."""
    from esphome.components import esp32_ble, esp32_ble_tracker

    if esp32_ble.IDF_MAX_CONNECTIONS != _IDF_MAX_CONNECTIONS:
        raise cv.Invalid(
            f"bluetooth_proxy's connection-slot limit mirror "
            f"({_IDF_MAX_CONNECTIONS}) is out of sync with "
            f"esp32_ble.IDF_MAX_CONNECTIONS ({esp32_ble.IDF_MAX_CONNECTIONS}); "
            f"update _IDF_MAX_CONNECTIONS in bluetooth_proxy/__init__.py"
        )

    CONNECTION_SCHEMA = bluetooth_connection.hub_connection_schema(PLATFORM_ESP32)

    def validate_connections(config: ConfigType) -> ConfigType:
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


@functools.cache
def _rp2_config_schema() -> cv.All:
    """Full proxy on the rp2 BLE hub: active connections through the BTstack
    GATT client backend in bluetooth_connection. Multi-slot builds replace the
    prebuilt library's one-client BTstack pools via linker --wrap, owned by
    rp2040_ble and requested when a second backend registers."""
    connection_schema = bluetooth_connection.hub_connection_schema(PLATFORM_RP2)

    def populate_connections(config: ConfigType) -> ConfigType:
        from esphome.components import rp2040_ble

        # One wrapper + backend pair per slot, declared during validation so
        # their ids exist for codegen (the esp32 arm's `connections` pattern).
        if not config[CONF_ACTIVE]:
            return config
        connection_slots: int = config[CONF_CONNECTION_SLOTS]
        rp2040_ble.consume_connection_slots(connection_slots, "bluetooth_proxy")(config)
        return {
            **config,
            CONF_CONNECTIONS: [connection_schema({}) for _ in range(connection_slots)],
        }

    max_conn = bluetooth_connection.HUB_MAX_CONNECTIONS[PLATFORM_RP2]
    schema = (
        cv.Schema(
            {
                **_COMMON_SCHEMA_KEYS,
                cv.Optional(CONF_ACTIVE, default=True): cv.boolean,
                cv.Optional(
                    CONF_CONNECTION_SLOTS,
                    default=min(DEFAULT_CONNECTION_SLOTS, max_conn),
                ): cv.All(
                    cv.positive_int,
                    cv.Range(
                        min=1,
                        max=max_conn,
                        msg=f"rp2 supports at most {max_conn} connection slot(s); "
                        "the BTstack pool overrides in rp2040_ble are sized "
                        f"for {max_conn}",
                    ),
                ),
            }
        )
        .extend(
            # ble_hub_id with the friendly no-tracker-configured guard.
            ble_device_base.BLE_DEVICE_SCHEMA
        )
        .extend(cv.COMPONENT_SCHEMA)
    )
    return cv.All(schema, populate_connections)


async def _connections_to_code(var: cg.MockObj, config: ConfigType) -> None:
    """One wrapper + backend pair per slot; the platform-specific backend
    registration lives in bluetooth_connection.new_gatt_backend()."""
    connections = config.get(CONF_CONNECTIONS, [])
    # The api component sizes BluetoothConnectionsFreeResponse.allocated with
    # this define whenever a proxy is present (zero on advertisement-only
    # hubs); sized here so it can never diverge from the loop below.
    cg.add_define("BLUETOOTH_PROXY_MAX_CONNECTIONS", len(connections))
    if connections:
        # Gates the connection and GATT half of the API surface. A proxy
        # without slots omits FEATURE_ACTIVE_CONNECTIONS, so a client never
        # sends those requests and their handlers and encoders are dead.
        cg.add_define("USE_BLUETOOTH_PROXY_CONNECTIONS")
    for connection_conf in connections:
        backend = await bluetooth_connection.new_gatt_backend(connection_conf)
        connection = cg.new_Pvariable(connection_conf[CONF_ID])
        cg.add(connection.set_backend(backend))
        cg.add(var.register_connection(connection))


# Per-platform schema builders; every key of
# bluetooth_connection.HUB_MAX_CONNECTIONS needs an entry here (pinned by
# tests/component_tests/bluetooth_proxy/). Connection codegen is shared.
_GATT_HUB_SCHEMAS = {PLATFORM_RP2: _rp2_config_schema}


# Keys every platform arm declares identically; each arm spreads this dict so
# the shared surface cannot drift. CONF_ACTIVE stays per-arm: its default
# differs (esp32 True, rp2 True, advertisement-only False).
_COMMON_SCHEMA_KEYS = {
    cv.GenerateID(): cv.declare_id(BluetoothProxy),
}

# Advertisement-only proxy on a neutral BLE hub: the hub's raw-advertisement
# callback feeds the same API batching, no connection stack compiled.
_BLE_HUB_CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            **_COMMON_SCHEMA_KEYS,
            cv.Optional(CONF_ACTIVE, default=False): cv.boolean,
        }
    )
    .extend(
        # ble_hub_id with the friendly no-tracker-configured guard.
        ble_device_base.BLE_DEVICE_SCHEMA
    )
    .extend(cv.COMPONENT_SCHEMA),
    _validate_no_active,
)


@schema_extractor("schema")
def _validate_platform(config: ConfigType) -> ConfigType:
    """Apply the schema for the platform actually being compiled.

    Three-way dispatch: esp32 gets the full GATT proxy, HUB_MAX_CONNECTIONS
    platforms get their _GATT_HUB_SCHEMAS arm, the remaining hub platforms get
    the advertisement-only shape; unsupported keys were already rejected by
    name in _reject_unsupported_connection_keys.
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
        full = ", ".join(["esp32", *sorted(bluetooth_connection.HUB_MAX_CONNECTIONS)])
        adv_only = ", ".join(
            sorted(set(_HUB_PLATFORMS) - set(bluetooth_connection.HUB_MAX_CONNECTIONS))
        )
        raise cv.Invalid(
            f"bluetooth_proxy is not supported on {CORE.target_platform}: no "
            "active-scan-capable BLE tracker hub is available for this "
            f"platform. It runs on {full} (full proxy) and {adv_only} "
            "(advertisement-only)."
        )
    if CORE.target_platform in bluetooth_connection.HUB_MAX_CONNECTIONS:
        return _GATT_HUB_SCHEMAS[CORE.target_platform]()(config)
    return _BLE_HUB_CONFIG_SCHEMA(config)


def _reject_unsupported_connection_keys(config: ConfigType) -> ConfigType:
    """Reject connection options a platform does not support, by name.

    GATT hub platforms keep connection_slots but reject the esp32-only keys;
    advertisement-only hubs reject all three. Runs before the walkable schema
    below so the user gets "this option does not exist here" instead of a
    value-range error implying the option works.
    """
    if not isinstance(config, dict) or CORE.is_esp32 or CORE.target_platform is None:
        return config
    if CORE.target_platform not in _HUB_PLATFORMS:
        # No proxy of any kind exists here: fall through so _validate_platform
        # reports "not supported on {platform}" instead of a key-level message
        # implying an advertisement-only proxy is available.
        return config
    if CORE.target_platform in bluetooth_connection.HUB_MAX_CONNECTIONS:
        # Full proxy: connection_slots is real here; the per-connection list
        # exists internally but carries no user options, and the Bluedroid
        # NVS service cache is esp32-only.
        rejected = {
            CONF_CONNECTIONS: (
                "has no per-connection options on this platform; use "
                "'connection_slots' to set the count"
            ),
            CONF_CACHE_SERVICES: "is esp32-only (Bluedroid NVS service cache)",
        }
    else:
        reason = (
            "requires active connection support; this platform runs the "
            "advertisement-only proxy and has no such option"
        )
        rejected = dict.fromkeys(
            (CONF_CONNECTION_SLOTS, CONF_CACHE_SERVICES, CONF_CONNECTIONS), reason
        )
    for key, reason in rejected.items():
        if key in config:
            raise cv.Invalid(f"'{key}' {reason}", path=[key])
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
    _reject_unsupported_connection_keys,
    cv.Schema(
        {
            cv.Optional(CONF_ACTIVE): cv.boolean,
            cv.Optional(CONF_CACHE_SERVICES): cv.boolean,
            # Bounded by the loosest platform cap so range walkers (the
            # device-builder field-range sync) see a real Range; the
            # per-platform schemas tighten it (1 on rp2) with their own error.
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
    tracker = await cg.get_variable(config[esp32_ble_tracker.CONF_ESP32_BLE_ID])
    cg.add(var.set_ble_hub(tracker))

    # Compiles the scanner-state push slot into the tracker and the matching
    # registration into the proxy; the other hubs are polled instead.
    cg.add_define("USE_BLE_SCANNER_STATE_CALLBACK")

    await _connections_to_code(var, config)

    if config.get(CONF_CACHE_SERVICES):
        add_idf_sdkconfig_option("CONFIG_BT_GATTC_CACHE_NVS_FLASH", True)


async def _to_code_ble_hub(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_active(config[CONF_ACTIVE]))
    hub = await cg.get_variable(config[ble_device_base.CONF_BLE_HUB_ID])
    cg.add(var.set_ble_hub(hub))

    await _connections_to_code(var, config)


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

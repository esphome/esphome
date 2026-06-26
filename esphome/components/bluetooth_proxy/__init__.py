import logging

import voluptuous as vol

import esphome.codegen as cg
from esphome.components import esp32_ble, esp32_ble_client, esp32_ble_tracker
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.components.esp32_ble import BTLoggers
import esphome.config_validation as cv
from esphome.const import CONF_ACTIVE, CONF_ID
from esphome.core import CORE

CODEOWNERS = ["@jesserockz", "@bdraco"]
DEPENDENCIES = ["api"]

_LOGGER = logging.getLogger(__name__)

CONF_CONNECTION_SLOTS = "connection_slots"
CONF_CACHE_SERVICES = "cache_services"
CONF_CONNECTIONS = "connections"
CONF_BLE_ID = "ble_id"
DEFAULT_CONNECTION_SLOTS = 3

# Single validator for connection_slots, shared by the ESP32 schema and the dashboard
# field-range introspection shim (_FIELD_RANGES_SCHEMA) so their bounds cannot drift apart.
_CONNECTION_SLOTS_VALIDATOR = cv.All(
    cv.positive_int,
    cv.Range(min=1, max=esp32_ble.IDF_MAX_CONNECTIONS),
)

bluetooth_proxy_ns = cg.esphome_ns.namespace("bluetooth_proxy")

# Same C++ class name on every platform — the implementation is selected with #ifdef.
# On ESP32 the proxy is an esp32_ble_tracker listener with GATT active connections; on
# LibreTiny it is an advertisement-only proxy fed by a tracker hub (BleProxyHub).
ESP32BluetoothProxy = bluetooth_proxy_ns.class_(
    "BluetoothProxy", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)
LibreTinyBluetoothProxy = bluetooth_proxy_ns.class_("BluetoothProxy", cg.Component)
BluetoothConnection = bluetooth_proxy_ns.class_(
    "BluetoothConnection", esp32_ble_client.BLEClientBase
)
# Hub interface implemented by LibreTiny trackers (bk72xx_ble_tracker, ln882h_ble_tracker).
BleProxyHub = bluetooth_proxy_ns.class_("BleProxyHub")


# AUTO_LOAD is a callable so it is evaluated once the target platform is known. The proxy
# pulls in the BLE tracker hub for the target chip — esp32_ble_tracker (plus the BLE client
# for GATT connections) on ESP32, or the matching LibreTiny tracker — so `bluetooth_proxy:`
# on its own is enough on every platform; the hub's id is then auto-resolved via ble_id.
def AUTO_LOAD():
    if CORE.is_esp32:
        return ["esp32_ble_client", "esp32_ble_tracker"]
    if CORE.is_bk72xx:
        return ["bk72xx_ble_tracker"]
    if CORE.is_ln882x:
        return ["ln882h_ble_tracker"]
    return []


# ---------------------------------------------------------------------------
# ESP32: advertisement proxy + GATT active connections
# ---------------------------------------------------------------------------
CONNECTION_SCHEMA = esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(BluetoothConnection),
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_esp32_connections(config):
    if CONF_CONNECTIONS in config:
        if not config[CONF_ACTIVE]:
            raise cv.Invalid(
                "Connections can only be used if the proxy is set to active"
            )
    elif config[CONF_ACTIVE]:
        connection_slots: int = config[CONF_CONNECTION_SLOTS]
        esp32_ble.consume_connection_slots(connection_slots, "bluetooth_proxy")(config)
        return {
            **config,
            CONF_CONNECTIONS: [CONNECTION_SCHEMA({}) for _ in range(connection_slots)],
        }
    return config


ESP32_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESP32BluetoothProxy),
            cv.Optional(CONF_ACTIVE, default=True): cv.boolean,
            cv.Optional(CONF_CACHE_SERVICES, default=True): cv.boolean,
            cv.Optional(
                CONF_CONNECTION_SLOTS, default=DEFAULT_CONNECTION_SLOTS
            ): _CONNECTION_SLOTS_VALIDATOR,
            cv.Optional(CONF_CONNECTIONS): cv.All(
                cv.ensure_list(CONNECTION_SCHEMA),
                cv.Length(min=1, max=esp32_ble.IDF_MAX_CONNECTIONS),
            ),
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    _validate_esp32_connections,
)

# ---------------------------------------------------------------------------
# LibreTiny: advertisement proxy only (GATT active connections are ESP32-only)
# ---------------------------------------------------------------------------
LIBRETINY_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LibreTinyBluetoothProxy),
        # The tracker hub to attach to (auto-injected when only one is declared).
        cv.GenerateID(CONF_BLE_ID): cv.use_id(BleProxyHub),
    }
).extend(cv.COMPONENT_SCHEMA)


# The platform is resolved at validation time (not import time) — the component is shared
# by ESP32 and LibreTiny and is loaded once per process.
def _bluetooth_proxy_schema(config):
    if CORE.is_esp32:
        return ESP32_SCHEMA(config)
    # Only the LibreTiny families that ship a BLE hub (bk72xx / ln882x) — not every
    # LibreTiny target — get the LibreTiny proxy schema.
    if CORE.is_libretiny and CORE.target_platform in ("bk72xx", "ln882x"):
        return LIBRETINY_SCHEMA(config)
    raise cv.Invalid(
        "bluetooth_proxy is only supported on ESP32 and LibreTiny BLE hubs (bk72xx / ln882h)"
    )


# Field-range introspection shim. Tooling (the dashboard visual editor) walks CONFIG_SCHEMA
# statically to recover numeric field bounds — e.g. connection_slots' Range — which a bare
# dispatch callable would hide. Wrapping the dispatcher in cv.All behind a permissive schema
# keeps those bounds visible without affecting validation: ALLOW_EXTRA and no defaults mean
# this shim never injects or rejects keys (it leaves the LibreTiny shape untouched), and the
# real per-platform validation runs in the dispatcher above.
_FIELD_RANGES_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CONNECTION_SLOTS): _CONNECTION_SLOTS_VALIDATOR,
    },
    extra=vol.ALLOW_EXTRA,
)

CONFIG_SCHEMA = cv.All(_FIELD_RANGES_SCHEMA, _bluetooth_proxy_schema)


async def _esp32_to_code(config):
    # Register the loggers this component needs.
    esp32_ble.register_bt_logger(BTLoggers.GATT, BTLoggers.L2CAP, BTLoggers.SMP)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_active(config[CONF_ACTIVE]))
    await esp32_ble_tracker.register_raw_ble_device(var, config)

    connection_count = len(config.get(CONF_CONNECTIONS, []))
    cg.add_define("BLUETOOTH_PROXY_MAX_CONNECTIONS", connection_count)
    cg.add_define("BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE", 16)

    for connection_conf in config.get(CONF_CONNECTIONS, []):
        connection_var = cg.new_Pvariable(connection_conf[CONF_ID])
        await cg.register_component(connection_var, connection_conf)
        cg.add(var.register_connection(connection_var))
        await esp32_ble_tracker.register_raw_client(connection_var, connection_conf)

    if config.get(CONF_CACHE_SERVICES):
        add_idf_sdkconfig_option("CONFIG_BT_GATTC_CACHE_NVS_FLASH", True)

    cg.add_define("USE_BLUETOOTH_PROXY")


async def _libretiny_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_BLE_ID])
    cg.add(var.set_hub(hub))

    cg.add_define("USE_BLUETOOTH_PROXY")
    # 0 connection slots = advertisement proxy only (no GATT on LibreTiny).
    cg.add_define("BLUETOOTH_PROXY_MAX_CONNECTIONS", 0)
    cg.add_define("BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE", 8)


async def to_code(config):
    if CORE.is_esp32:
        await _esp32_to_code(config)
    else:
        await _libretiny_to_code(config)

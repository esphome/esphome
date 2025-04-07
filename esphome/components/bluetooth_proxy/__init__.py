import esphome.codegen as cg
from esphome.components import esp32_ble_client, esp32_ble_tracker
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import CONF_ACTIVE, CONF_ID

AUTO_LOAD = ["esp32_ble_client", "esp32_ble_tracker"]
DEPENDENCIES = ["api", "esp32"]
CODEOWNERS = ["@jesserockz"]

CONF_CONNECTION_SLOTS = "connection_slots"
CONF_CACHE_SERVICES = "cache_services"
CONF_CONNECTIONS = "connections"
DEFAULT_CONNECTION_SLOTS = 3

bluetooth_proxy_ns = cg.esphome_ns.namespace("bluetooth_proxy")

BluetoothProxy = bluetooth_proxy_ns.class_(
    "BluetoothProxy", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
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
        esp32_ble_tracker.consume_connection_slots(connection_slots, "bluetooth_proxy")(
            config
        )
        return {
            **config,
            CONF_CONNECTIONS: [CONNECTION_SCHEMA({}) for _ in range(connection_slots)],
        }
    return config


CONFIG_SCHEMA = cv.All(
    (
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(BluetoothProxy),
                cv.Optional(CONF_ACTIVE, default=False): cv.boolean,
                cv.SplitDefault(CONF_CACHE_SERVICES, esp32_idf=True): cv.All(
                    cv.only_with_esp_idf, cv.boolean
                ),
                cv.Optional(
                    CONF_CONNECTION_SLOTS,
                    default=DEFAULT_CONNECTION_SLOTS,
                ): cv.All(
                    cv.positive_int,
                    cv.Range(min=1, max=esp32_ble_tracker.max_connections()),
                ),
                cv.Optional(CONF_CONNECTIONS): cv.All(
                    cv.ensure_list(CONNECTION_SCHEMA),
                    cv.Length(min=1, max=esp32_ble_tracker.max_connections()),
                ),
            }
        )
        .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
        .extend(cv.COMPONENT_SCHEMA)
    ),
    validate_connections,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_active(config[CONF_ACTIVE]))
    await esp32_ble_tracker.register_ble_device(var, config)

    for connection_conf in config.get(CONF_CONNECTIONS, []):
        connection_var = cg.new_Pvariable(connection_conf[CONF_ID])
        await cg.register_component(connection_var, connection_conf)
        cg.add(var.register_connection(connection_var))
        await esp32_ble_tracker.register_client(connection_var, connection_conf)

    if config.get(CONF_CACHE_SERVICES):
        add_idf_sdkconfig_option("CONFIG_BT_GATTC_CACHE_NVS_FLASH", True)

    cg.add_define("USE_BLUETOOTH_PROXY")

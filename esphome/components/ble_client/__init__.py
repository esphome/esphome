import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble_tracker
from esphome.const import (
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_NAME,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_TRIGGER_ID,
)
from esphome import automation

CODEOWNERS = ["@buxtronix"]
DEPENDENCIES = ["esp32_ble_tracker"]

ble_client_ns = cg.esphome_ns.namespace("ble_client")
BLEClient = ble_client_ns.class_(
    "BLEClient", cg.Component, esp32_ble_tracker.ESPBTClient
)
BLEClientNode = ble_client_ns.class_("BLEClientNode")
BLEClientNodeConstRef = BLEClientNode.operator("ref").operator("const")
# Triggers
BLEClientConnectTrigger = ble_client_ns.class_(
    "BLEClientConnectTrigger", automation.Trigger.template(BLEClientNodeConstRef)
)
BLEClientDisconnectTrigger = ble_client_ns.class_(
    "BLEClientDisconnectTrigger", automation.Trigger.template(BLEClientNodeConstRef)
)

# Espressif platformio framework is built with MAX_BLE_CONN to 3, so
# enforce this in yaml checks.
MULTI_CONF = 3

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLEClient),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_NAME): cv.string,
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        BLEClientConnectTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        BLEClientDisconnectTrigger
                    ),
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
)

CONF_BLE_CLIENT_ID = "ble_client_id"

BLE_CLIENT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_BLE_CLIENT_ID): cv.use_id(BLEClient),
    }
)


async def register_ble_node(var, config):
    parent = await cg.get_variable(config[CONF_BLE_CLIENT_ID])
    cg.add(parent.register_ble_node(var))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_client(var, config)
    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    for conf in config.get(CONF_ON_CONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_DISCONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble_tracker, esp32_ble_client
from esphome.const import (
    CONF_CHARACTERISTIC_UUID,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_NAME,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_SERVICE_UUID,
    CONF_TRIGGER_ID,
    CONF_VALUE,
)
from esphome import automation

AUTO_LOAD = ["esp32_ble_client"]
CODEOWNERS = ["@buxtronix"]
DEPENDENCIES = ["esp32_ble_tracker"]

ble_client_ns = cg.esphome_ns.namespace("ble_client")
BLEClient = ble_client_ns.class_("BLEClient", esp32_ble_client.BLEClientBase)
BLEClientNode = ble_client_ns.class_("BLEClientNode")
BLEClientNodeConstRef = BLEClientNode.operator("ref").operator("const")
# Triggers
BLEClientConnectTrigger = ble_client_ns.class_(
    "BLEClientConnectTrigger", automation.Trigger.template(BLEClientNodeConstRef)
)
BLEClientDisconnectTrigger = ble_client_ns.class_(
    "BLEClientDisconnectTrigger", automation.Trigger.template(BLEClientNodeConstRef)
)
# Actions
BLEWriteAction = ble_client_ns.class_("BLEClientWriteAction", automation.Action)

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


BLE_WRITE_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(BLEClient),
        cv.Required(CONF_SERVICE_UUID): esp32_ble_tracker.bt_uuid,
        cv.Required(CONF_CHARACTERISTIC_UUID): esp32_ble_tracker.bt_uuid,
        cv.Required(CONF_VALUE): cv.templatable(cv.ensure_list(cv.hex_uint8_t)),
    }
)


@automation.register_action(
    "ble_client.ble_write", BLEWriteAction, BLE_WRITE_ACTION_SCHEMA
)
async def ble_write_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    value = config[CONF_VALUE]
    if cg.is_template(value):
        templ = await cg.templatable(value, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_value_template(templ))
    else:
        cg.add(var.set_value_simple(value))

    if len(config[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid16_format):
        cg.add(
            var.set_service_uuid16(esp32_ble_tracker.as_hex(config[CONF_SERVICE_UUID]))
        )
    elif len(config[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid32_format):
        cg.add(
            var.set_service_uuid32(esp32_ble_tracker.as_hex(config[CONF_SERVICE_UUID]))
        )
    elif len(config[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid128_format):
        uuid128 = esp32_ble_tracker.as_reversed_hex_array(config[CONF_SERVICE_UUID])
        cg.add(var.set_service_uuid128(uuid128))

    if len(config[CONF_CHARACTERISTIC_UUID]) == len(esp32_ble_tracker.bt_uuid16_format):
        cg.add(
            var.set_char_uuid16(
                esp32_ble_tracker.as_hex(config[CONF_CHARACTERISTIC_UUID])
            )
        )
    elif len(config[CONF_CHARACTERISTIC_UUID]) == len(
        esp32_ble_tracker.bt_uuid32_format
    ):
        cg.add(
            var.set_char_uuid32(
                esp32_ble_tracker.as_hex(config[CONF_CHARACTERISTIC_UUID])
            )
        )
    elif len(config[CONF_CHARACTERISTIC_UUID]) == len(
        esp32_ble_tracker.bt_uuid128_format
    ):
        uuid128 = esp32_ble_tracker.as_reversed_hex_array(
            config[CONF_CHARACTERISTIC_UUID]
        )
        cg.add(var.set_char_uuid128(uuid128))

    return var


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

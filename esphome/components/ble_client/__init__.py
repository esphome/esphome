import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.automation import maybe_simple_id
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
CODEOWNERS = ["@buxtronix", "@clydebarrow"]
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
BLEClientPasskeyRequestTrigger = ble_client_ns.class_(
    "BLEClientPasskeyRequestTrigger", automation.Trigger.template(BLEClientNodeConstRef)
)
BLEClientPasskeyNotificationTrigger = ble_client_ns.class_(
    "BLEClientPasskeyNotificationTrigger",
    automation.Trigger.template(BLEClientNodeConstRef, cg.uint32),
)
BLEClientNumericComparisonRequestTrigger = ble_client_ns.class_(
    "BLEClientNumericComparisonRequestTrigger",
    automation.Trigger.template(BLEClientNodeConstRef, cg.uint32),
)

# Actions
BLEWriteAction = ble_client_ns.class_("BLEClientWriteAction", automation.Action)
BLEConnectAction = ble_client_ns.class_("BLEClientConnectAction", automation.Action)
BLEDisconnectAction = ble_client_ns.class_(
    "BLEClientDisconnectAction", automation.Action
)
BLEPasskeyReplyAction = ble_client_ns.class_(
    "BLEClientPasskeyReplyAction", automation.Action
)
BLENumericComparisonReplyAction = ble_client_ns.class_(
    "BLEClientNumericComparisonReplyAction", automation.Action
)
BLERemoveBondAction = ble_client_ns.class_(
    "BLEClientRemoveBondAction", automation.Action
)

CONF_PASSKEY = "passkey"
CONF_ACCEPT = "accept"
CONF_ON_PASSKEY_REQUEST = "on_passkey_request"
CONF_ON_PASSKEY_NOTIFICATION = "on_passkey_notification"
CONF_ON_NUMERIC_COMPARISON_REQUEST = "on_numeric_comparison_request"
CONF_AUTO_CONNECT = "auto_connect"

# Espressif platformio framework is built with MAX_BLE_CONN to 3, so
# enforce this in yaml checks.
MULTI_CONF = 3

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLEClient),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_NAME): cv.string,
            cv.Optional(CONF_AUTO_CONNECT, default=True): cv.boolean,
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
            cv.Optional(CONF_ON_PASSKEY_REQUEST): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        BLEClientPasskeyRequestTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_PASSKEY_NOTIFICATION): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        BLEClientPasskeyNotificationTrigger
                    ),
                }
            ),
            cv.Optional(
                CONF_ON_NUMERIC_COMPARISON_REQUEST
            ): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        BLEClientNumericComparisonRequestTrigger
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
        cv.GenerateID(CONF_BLE_CLIENT_ID): cv.use_id(BLEClient),
    }
)


async def register_ble_node(var, config):
    parent = await cg.get_variable(config[CONF_BLE_CLIENT_ID])
    cg.add(parent.register_ble_node(var))


BLE_WRITE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(BLEClient),
        cv.Required(CONF_SERVICE_UUID): esp32_ble_tracker.bt_uuid,
        cv.Required(CONF_CHARACTERISTIC_UUID): esp32_ble_tracker.bt_uuid,
        cv.Required(CONF_VALUE): cv.templatable(cv.ensure_list(cv.hex_uint8_t)),
    }
)

BLE_CONNECT_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(CONF_ID): cv.use_id(BLEClient),
    }
)

BLE_NUMERIC_COMPARISON_REPLY_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(BLEClient),
        cv.Required(CONF_ACCEPT): cv.templatable(cv.boolean),
    }
)

BLE_PASSKEY_REPLY_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(BLEClient),
        cv.Required(CONF_PASSKEY): cv.templatable(cv.int_range(min=0, max=999999)),
    }
)


BLE_REMOVE_BOND_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(BLEClient),
    }
)


@automation.register_action(
    "ble_client.disconnect", BLEDisconnectAction, BLE_CONNECT_ACTION_SCHEMA
)
async def ble_disconnect_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var


@automation.register_action(
    "ble_client.connect", BLEConnectAction, BLE_CONNECT_ACTION_SCHEMA
)
async def ble_connect_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var


@automation.register_action(
    "ble_client.ble_write", BLEWriteAction, BLE_WRITE_ACTION_SCHEMA
)
async def ble_write_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

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


@automation.register_action(
    "ble_client.numeric_comparison_reply",
    BLENumericComparisonReplyAction,
    BLE_NUMERIC_COMPARISON_REPLY_ACTION_SCHEMA,
)
async def numeric_comparison_reply_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    accept = config[CONF_ACCEPT]
    if cg.is_template(accept):
        templ = await cg.templatable(accept, args, cg.bool_)
        cg.add(var.set_value_template(templ))
    else:
        cg.add(var.set_value_simple(accept))

    return var


@automation.register_action(
    "ble_client.passkey_reply", BLEPasskeyReplyAction, BLE_PASSKEY_REPLY_ACTION_SCHEMA
)
async def passkey_reply_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    passkey = config[CONF_PASSKEY]
    if cg.is_template(passkey):
        templ = await cg.templatable(passkey, args, cg.uint32)
        cg.add(var.set_value_template(templ))
    else:
        cg.add(var.set_value_simple(passkey))

    return var


@automation.register_action(
    "ble_client.remove_bond",
    BLERemoveBondAction,
    BLE_REMOVE_BOND_ACTION_SCHEMA,
)
async def remove_bond_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_client(var, config)
    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_auto_connect(config[CONF_AUTO_CONNECT]))
    for conf in config.get(CONF_ON_CONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_DISCONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_PASSKEY_REQUEST, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_PASSKEY_NOTIFICATION, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "passkey")], conf)
    for conf in config.get(CONF_ON_NUMERIC_COMPARISON_REQUEST, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "passkey")], conf)

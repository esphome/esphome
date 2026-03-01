from esphome import automation
import esphome.codegen as cg
from esphome.components.zephyr import zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TRIGGER_ID, Framework
from esphome.core import CORE

zephyr_ble_server_ns = cg.esphome_ns.namespace("zephyr_ble_server")
BLEServer = zephyr_ble_server_ns.class_("BLEServer", cg.Component)

BLENumericComparisonRequestTrigger = zephyr_ble_server_ns.class_(
    "BLENumericComparisonRequestTrigger",
    automation.Trigger.template(cg.uint32),
)

CONF_ON_NUMERIC_COMPARISON_REQUEST = "on_numeric_comparison_request"
CONF_ACCEPT = "accept"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLEServer),
            cv.Optional(
                CONF_ON_NUMERIC_COMPARISON_REQUEST
            ): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        BLENumericComparisonRequestTrigger
                    ),
                }
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_framework(Framework.ZEPHYR),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    zephyr_add_prj_conf("BT", True)
    zephyr_add_prj_conf("BT_PERIPHERAL", True)
    zephyr_add_prj_conf("BT_RX_STACK_SIZE", 1536)
    zephyr_add_prj_conf("BT_DEVICE_NAME", CORE.name)
    await cg.register_component(var, config)
    for conf in config.get(CONF_ON_NUMERIC_COMPARISON_REQUEST, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "passkey")], conf)
    if len(config.get(CONF_ON_NUMERIC_COMPARISON_REQUEST, [])) > 0:
        zephyr_add_prj_conf("BT_SMP", True)
        zephyr_add_prj_conf("BT_SETTINGS", True)
        zephyr_add_prj_conf("BT_SMP_SC_ONLY", True)
        zephyr_add_prj_conf("BT_KEYS_OVERWRITE_OLDEST", True)


BLENumericComparisonReplyAction = zephyr_ble_server_ns.class_(
    "BLENumericComparisonReplyAction", automation.Action
)

BLE_NUMERIC_COMPARISON_REPLY_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(BLEServer),
        cv.Required(CONF_ACCEPT): cv.templatable(cv.boolean),
    }
)


@automation.register_action(
    "ble_server.numeric_comparison_reply",
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

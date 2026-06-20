from esphome import automation
import esphome.codegen as cg
from esphome.components.zephyr import zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import CONF_ID, Framework
from esphome.core import CORE

zephyr_ble_server_ns = cg.esphome_ns.namespace("zephyr_ble_server")
BLEServer = zephyr_ble_server_ns.class_("BLEServer", cg.Component)

CONF_ON_NUMERIC_COMPARISON_REQUEST = "on_numeric_comparison_request"
CONF_ACCEPT = "accept"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLEServer),
            cv.Optional(
                CONF_ON_NUMERIC_COMPARISON_REQUEST
            ): automation.validate_automation({}),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_framework(Framework.ZEPHYR),
)

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_NUMERIC_COMPARISON_REQUEST,
        "add_passkey_callback",
        [(cg.uint32, "passkey")],
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    zephyr_add_prj_conf("BT", True)
    zephyr_add_prj_conf("BT_PERIPHERAL", True)
    zephyr_add_prj_conf("BT_RX_STACK_SIZE", 1536)
    zephyr_add_prj_conf("BT_DEVICE_NAME", CORE.name)
    # Raise the ATT MTU and enable LL Data Length Extension so each notification can carry
    # ~244 bytes of payload instead of the default 20. The central (e.g. macOS) requests a large
    # MTU on connect; the peripheral must advertise a matching BT_L2CAP_TX_MTU and provide ACL
    # buffers large enough to hold MTU + 4 bytes of L2CAP header for the negotiation to take.
    # This is the single biggest throughput win for BLE log streaming and OTA, shrinking a burst
    # from dozens of round-trips to a handful.
    zephyr_add_prj_conf("BT_L2CAP_TX_MTU", 247)
    zephyr_add_prj_conf("BT_BUF_ACL_TX_SIZE", 251)
    zephyr_add_prj_conf("BT_BUF_ACL_RX_SIZE", 251)
    zephyr_add_prj_conf("BT_CTLR_DATA_LENGTH_MAX", 251)
    zephyr_add_prj_conf("BT_USER_DATA_LEN_UPDATE", True)
    zephyr_add_prj_conf("BT_AUTO_DATA_LEN_UPDATE", True)
    await cg.register_component(var, config)
    if config.get(CONF_ON_NUMERIC_COMPARISON_REQUEST):
        zephyr_add_prj_conf("BT_SMP", True)
        zephyr_add_prj_conf("BT_SETTINGS", True)
        zephyr_add_prj_conf("BT_SMP_SC_ONLY", True)
        zephyr_add_prj_conf("BT_KEYS_OVERWRITE_OLDEST", True)
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


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
    synchronous=True,
)
async def numeric_comparison_reply_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    templ = await cg.templatable(config[CONF_ACCEPT], args, cg.bool_)
    cg.add(var.set_accept(templ))

    return var

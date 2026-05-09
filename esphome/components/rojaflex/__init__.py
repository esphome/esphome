import esphome.codegen as cg
from esphome import automation
from esphome.components import cc1101
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_COMMAND, CONF_ID

CODEOWNERS = ["@sebastianhofmann"]
DEPENDENCIES = ["cc1101"]

CONF_ROJAFLEX_ID = "rojaflex_id"
CONF_CC1101_ID = "cc1101_id"
CONF_HOUSECODE = "housecode"
CONF_TX_REPETITIONS = "tx_repetitions"
CONF_TARGET_PERCENT = "target_percent"

COMMANDS = {
    "stop": 0,
    "up": 1,
    "down": 8,
}

rojaflex_ns = cg.esphome_ns.namespace("rojaflex")
RojaflexComponent = rojaflex_ns.class_("RojaflexComponent", cg.Component)
RojaflexDevice = rojaflex_ns.class_(
    "RojaflexDevice", cg.Parented.template(RojaflexComponent)
)


def validate_housecode(value):
    if len(value) != 7:
        raise cv.Invalid("housecode must be exactly 7 hex chars")
    if any(c not in "0123456789abcdefABCDEF" for c in value):
        raise cv.Invalid("housecode must be hexadecimal")
    return value.upper()


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RojaflexComponent),
        cv.Required(CONF_CC1101_ID): cv.use_id(cc1101.CC1101Component),
        cv.Optional(CONF_HOUSECODE, default="0000000"): validate_housecode,
        cv.Optional(CONF_TX_REPETITIONS, default=2): cv.int_range(min=1, max=9),
    }
).extend(cv.COMPONENT_SCHEMA)

ROJAFLEX_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ROJAFLEX_ID): cv.use_id(RojaflexComponent),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    transceiver = await cg.get_variable(config[CONF_CC1101_ID])
    cg.add(var.set_transceiver(transceiver))
    cg.add(var.set_housecode(config[CONF_HOUSECODE]))
    cg.add(var.set_tx_repetitions(config[CONF_TX_REPETITIONS]))


async def register_rojaflex_device(var, config):
    parent = await cg.get_variable(config[CONF_ROJAFLEX_ID])
    cg.add(var.set_parent(parent))
    cg.add(parent.register_device(var))


SetHousecodeAction = rojaflex_ns.class_(
    "SetHousecodeAction", automation.Action, cg.Parented.template(RojaflexComponent)
)
SendCommandAction = rojaflex_ns.class_(
    "SendCommandAction", automation.Action, cg.Parented.template(RojaflexComponent)
)
SetPositionAction = rojaflex_ns.class_(
    "SetPositionAction", automation.Action, cg.Parented.template(RojaflexComponent)
)


@automation.register_action(
    "rojaflex.set_housecode",
    SetHousecodeAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_ROJAFLEX_ID): cv.use_id(RojaflexComponent),
            cv.Required(CONF_HOUSECODE): cv.templatable(validate_housecode),
        }
    ),
    synchronous=True,
)
async def set_housecode_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ROJAFLEX_ID])
    templ = await cg.templatable(config[CONF_HOUSECODE], args, cg.std_string)
    cg.add(var.set_housecode(templ))
    return var


@automation.register_action(
    "rojaflex.send_command",
    SendCommandAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_ROJAFLEX_ID): cv.use_id(RojaflexComponent),
            cv.Required(CONF_CHANNEL): cv.templatable(cv.int_range(min=0, max=15)),
            cv.Required(CONF_COMMAND): cv.templatable(cv.enum(COMMANDS, lower=True)),
        }
    ),
    synchronous=True,
)
async def send_command_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ROJAFLEX_ID])
    channel = await cg.templatable(config[CONF_CHANNEL], args, cg.uint8)
    cmd = await cg.templatable(config[CONF_COMMAND], args, cg.uint8)
    cg.add(var.set_channel(channel))
    cg.add(var.set_command(cmd))
    return var


@automation.register_action(
    "rojaflex.set_position",
    SetPositionAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_ROJAFLEX_ID): cv.use_id(RojaflexComponent),
            cv.Required(CONF_CHANNEL): cv.templatable(cv.int_range(min=0, max=15)),
            cv.Required(CONF_TARGET_PERCENT): cv.templatable(cv.int_range(min=0, max=100)),
        }
    ),
    synchronous=True,
)
async def set_position_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ROJAFLEX_ID])
    channel = await cg.templatable(config[CONF_CHANNEL], args, cg.uint8)
    target = await cg.templatable(config[CONF_TARGET_PERCENT], args, cg.int_)
    cg.add(var.set_channel(channel))
    cg.add(var.set_target(target))
    return var

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import power_supply
from esphome.const import (
    CONF_ID,
    CONF_INVERTED,
    CONF_LEVEL,
    CONF_MAX_POWER,
    CONF_MIN_POWER,
    CONF_POWER_SUPPLY,
)
from esphome.core import CORE


CODEOWNERS = ["@esphome/core"]
IS_PLATFORM_COMPONENT = True

CONF_ZERO_MEANS_ZERO = "zero_means_zero"

BINARY_OUTPUT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_POWER_SUPPLY): cv.use_id(power_supply.PowerSupply),
        cv.Optional(CONF_INVERTED): cv.boolean,
    }
)

FLOAT_OUTPUT_SCHEMA = BINARY_OUTPUT_SCHEMA.extend(
    {
        cv.Optional(CONF_MAX_POWER): cv.percentage,
        cv.Optional(CONF_MIN_POWER): cv.percentage,
        cv.Optional(CONF_ZERO_MEANS_ZERO, default=False): cv.boolean,
    }
)

output_ns = cg.esphome_ns.namespace("output")
BinaryOutput = output_ns.class_("BinaryOutput")
BinaryOutputPtr = BinaryOutput.operator("ptr")
FloatOutput = output_ns.class_("FloatOutput", BinaryOutput)
FloatOutputPtr = FloatOutput.operator("ptr")

# Actions
TurnOffAction = output_ns.class_("TurnOffAction", automation.Action)
TurnOnAction = output_ns.class_("TurnOnAction", automation.Action)
SetLevelAction = output_ns.class_("SetLevelAction", automation.Action)


async def setup_output_platform_(obj, config):
    if CONF_INVERTED in config:
        cg.add(obj.set_inverted(config[CONF_INVERTED]))
    if CONF_POWER_SUPPLY in config:
        power_supply_ = await cg.get_variable(config[CONF_POWER_SUPPLY])
        cg.add(obj.set_power_supply(power_supply_))
    if CONF_MAX_POWER in config:
        cg.add(obj.set_max_power(config[CONF_MAX_POWER]))
    if CONF_MIN_POWER in config:
        cg.add(obj.set_min_power(config[CONF_MIN_POWER]))
    if CONF_ZERO_MEANS_ZERO in config:
        cg.add(obj.set_zero_means_zero(config[CONF_ZERO_MEANS_ZERO]))


async def register_output(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    await setup_output_platform_(var, config)


BINARY_OUTPUT_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(BinaryOutput),
    }
)


@automation.register_action("output.turn_on", TurnOnAction, BINARY_OUTPUT_ACTION_SCHEMA)
async def output_turn_on_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "output.turn_off", TurnOffAction, BINARY_OUTPUT_ACTION_SCHEMA
)
async def output_turn_off_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "output.set_level",
    SetLevelAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(FloatOutput),
            cv.Required(CONF_LEVEL): cv.templatable(cv.percentage),
        }
    ),
)
async def output_set_level_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_LEVEL], args, float)
    cg.add(var.set_level(template_))
    return var


async def to_code(config):
    cg.add_define("USE_OUTPUT")
    cg.add_global(output_ns.using)

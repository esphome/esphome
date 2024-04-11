import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import fan, output
from esphome.components.fan import validate_preset_modes
from esphome.const import (
    CONF_ID,
    CONF_DECAY_MODE,
    CONF_SPEED_COUNT,
    CONF_PIN_A,
    CONF_PIN_B,
    CONF_ENABLE_PIN,
    CONF_PRESET_MODES,
)
from .. import hbridge_ns


CODEOWNERS = ["@WeekendWarrior"]


HBridgeFan = hbridge_ns.class_("HBridgeFan", cg.Component, fan.Fan)

DecayMode = hbridge_ns.enum("DecayMode")
DECAY_MODE_OPTIONS = {
    "SLOW": DecayMode.DECAY_MODE_SLOW,
    "FAST": DecayMode.DECAY_MODE_FAST,
}

# Actions
BrakeAction = hbridge_ns.class_("BrakeAction", automation.Action)

CONFIG_SCHEMA = fan.FAN_SCHEMA.extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(HBridgeFan),
        cv.Required(CONF_PIN_A): cv.use_id(output.FloatOutput),
        cv.Required(CONF_PIN_B): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_DECAY_MODE, default="SLOW"): cv.enum(
            DECAY_MODE_OPTIONS, upper=True
        ),
        cv.Optional(CONF_SPEED_COUNT, default=100): cv.int_range(min=1),
        cv.Optional(CONF_ENABLE_PIN): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_PRESET_MODES): validate_preset_modes,
    }
).extend(cv.COMPONENT_SCHEMA)


@automation.register_action(
    "fan.hbridge.brake",
    BrakeAction,
    maybe_simple_id({cv.Required(CONF_ID): cv.use_id(HBridgeFan)}),
)
async def fan_hbridge_brake_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_SPEED_COUNT],
        config[CONF_DECAY_MODE],
    )
    await cg.register_component(var, config)
    await fan.register_fan(var, config)
    pin_a_ = await cg.get_variable(config[CONF_PIN_A])
    cg.add(var.set_pin_a(pin_a_))
    pin_b_ = await cg.get_variable(config[CONF_PIN_B])
    cg.add(var.set_pin_b(pin_b_))

    if CONF_ENABLE_PIN in config:
        enable_pin = await cg.get_variable(config[CONF_ENABLE_PIN])
        cg.add(var.set_enable_pin(enable_pin))

    if CONF_PRESET_MODES in config:
        cg.add(var.set_preset_modes(config[CONF_PRESET_MODES]))

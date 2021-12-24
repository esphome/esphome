import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import fan, output
from esphome.const import (
    CONF_ID,
    CONF_SPEED_COUNT,
    CONF_PIN_A,
    CONF_PIN_B,
    CONF_ENABLE_PIN,
    CONF_OSCILLATION_OUTPUT,
)
from .. import hbridge_ns, HBRIDGE_CONFIG_SCHEMA


CODEOWNERS = ["@WeekendWarrior"]
AUTO_LOAD = ["hbridge"]


HBridgeFan = hbridge_ns.class_("HBridgeFan", cg.Component, fan.Fan)

CONFIG_SCHEMA = fan.FAN_SCHEMA.extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(HBridgeFan),
        cv.Optional(CONF_SPEED_COUNT, default=100): cv.int_range(min=1),
        cv.Optional(CONF_OSCILLATION_OUTPUT): cv.use_id(output.BinaryOutput),
    }
).extend(cv.COMPONENT_SCHEMA).extend(HBRIDGE_CONFIG_SCHEMA)

# Actions
BrakeAction = hbridge_ns.class_("BrakeAction", automation.Action)

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
    )
    await cg.register_component(var, config)
    await fan.register_fan(var, config)
    
    if CONF_OSCILLATION_OUTPUT in config:
       oscillation_output = await cg.get_variable(config[CONF_OSCILLATION_OUTPUT])
       cg.add(var.set_oscillation_output(oscillation_output))

    # HBridge driver config
    pina = await cg.get_variable(config[CONF_PIN_A])
    cg.add(var.set_hbridge_pin_a(pina))
    pinb = await cg.get_variable(config[CONF_PIN_B])
    cg.add(var.set_hbridge_pin_b(pinb))

    if CONF_ENABLE_PIN in config:
        pin_enable = await cg.get_variable(config[CONF_ENABLE_PIN])
        cg.add(var.set_hbridge_enable_pin(pin_enable))

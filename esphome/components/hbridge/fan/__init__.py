import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan, output
from esphome.const import (
    CONF_DECAY_MODE,
    CONF_OUTPUT_ID,
    CONF_SPEED_COUNT,
    CONF_PIN_A,
    CONF_PIN_B
)
from .. import hbridge_ns

HBridgeFan = hbridge_ns.class_("HBridgeFan", cg.Component)

DecayMode = hbridge_ns.enum("DecayMode")
DECAY_MODE_OPTIONS = {
    "SLOW": DecayMode.DECAY_MODE_SLOW,
    "FAST": DecayMode.DECAY_MODE_FAST,
}


CONFIG_SCHEMA = fan.FAN_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(HBridgeFan),
        cv.Required(CONF_PIN_A): cv.use_id(output.FloatOutput),
        cv.Required(CONF_PIN_B): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_DECAY_MODE, default="SLOW"): cv.enum(
            DECAY_MODE_OPTIONS, upper=True
        ),
        cv.Optional(CONF_SPEED_COUNT, default=100): cv.int_range(min=1),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    pin_a_ = await cg.get_variable(config[CONF_PIN_A])
    pin_b_ = await cg.get_variable(config[CONF_PIN_B])
    state = await fan.create_fan_state(config)
    var = cg.new_Pvariable(
        config[CONF_OUTPUT_ID], state, pin_a_, pin_b_, config[CONF_SPEED_COUNT], DECAY_MODE_OPTIONS[config[CONF_DECAY_MODE]]
    )
    await cg.register_component(var, config)

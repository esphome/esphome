import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan, output
from esphome.const import CONF_OSCILLATION_OUTPUT, CONF_OUTPUT, CONF_DIRECTION_OUTPUT, \
    CONF_OUTPUT_ID, CONF_SPEED, CONF_LOW, CONF_MEDIUM, CONF_HIGH
from .. import speed_ns

SpeedFan = speed_ns.class_('SpeedFan', cg.Component)

CONFIG_SCHEMA = fan.FAN_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(SpeedFan),
    cv.Required(CONF_OUTPUT): cv.use_id(output.FloatOutput),
    cv.Optional(CONF_OSCILLATION_OUTPUT): cv.use_id(output.BinaryOutput),
    cv.Optional(CONF_DIRECTION_OUTPUT): cv.use_id(output.BinaryOutput),
    cv.Optional(CONF_SPEED, default={}): cv.Schema({
        cv.Optional(CONF_LOW, default=0.33): cv.percentage,
        cv.Optional(CONF_MEDIUM, default=0.66): cv.percentage,
        cv.Optional(CONF_HIGH, default=1.0): cv.percentage,
    }),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    output_ = yield cg.get_variable(config[CONF_OUTPUT])
    state = yield fan.create_fan_state(config)
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], state, output_)
    yield cg.register_component(var, config)
    speeds = config[CONF_SPEED]
    cg.add(var.set_speeds(speeds[CONF_LOW], speeds[CONF_MEDIUM], speeds[CONF_HIGH]))

    if CONF_OSCILLATION_OUTPUT in config:
        oscillation_output = yield cg.get_variable(config[CONF_OSCILLATION_OUTPUT])
        cg.add(var.set_oscillating(oscillation_output))

    if CONF_DIRECTION_OUTPUT in config:
        direction_output = yield cg.get_variable(config[CONF_DIRECTION_OUTPUT])
        cg.add(var.set_direction(direction_output))

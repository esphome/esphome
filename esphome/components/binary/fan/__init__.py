import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan, output
from esphome.const import CONF_OSCILLATION_OUTPUT, CONF_OUTPUT, \
    CONF_OUTPUT_ID
from .. import binary_ns

BinaryFan = binary_ns.class_('BinaryFan', cg.Component)

PLATFORM_SCHEMA = cv.nameable(fan.FAN_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_variable_id(BinaryFan),
    cv.Required(CONF_OUTPUT): cv.use_variable_id(output.BinaryOutput),
    cv.Optional(CONF_OSCILLATION_OUTPUT): cv.use_variable_id(output.BinaryOutput),
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    output_ = yield cg.get_variable(config[CONF_OUTPUT])
    state = yield fan.create_fan_state(config)
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], state, output_)
    yield cg.register_component(var, config)

    if CONF_OSCILLATION_OUTPUT in config:
        oscillation_output = yield cg.get_variable(config[CONF_OSCILLATION_OUTPUT])
        cg.add(var.set_oscillation(oscillation_output))

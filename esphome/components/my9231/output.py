import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.components.my9231 import MY9231OutputComponent
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_MY9231_ID

DEPENDENCIES = ['my9231']

Channel = MY9231OutputComponent.class_('Channel', output.FloatOutput)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.GenerateID(CONF_MY9231_ID): cv.use_variable_id(MY9231OutputComponent),

    cv.Required(CONF_ID): cv.declare_variable_id(Channel),
    cv.Required(CONF_CHANNEL): cv.All(cv.int_, cv.Range(min=0, max=65535)),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    my9231 = yield cg.get_variable(config[CONF_MY9231_ID])
    var = cg.new_Pvariable(config[CONF_ID], my9231, config[CONF_CHANNEL])
    yield output.register_output(var, config)

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_CHANNEL, CONF_ID
from . import SX1509Component, sx1509_ns

DEPENDENCIES = ['sx1509']

SX1509FloatOutputChannel = sx1509_ns.class_('SX1509FloatOutputChannel', output.FloatOutput)
CONF_SX1509_ID = 'sx1509_id'

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(SX1509FloatOutputChannel),
    cv.GenerateID(CONF_SX1509_ID): cv.use_id(SX1509Component),

    cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=15),
})


def to_code(config):
    paren = yield cg.get_variable(config[CONF_SX1509_ID])
    rhs = paren.create_float_output_channel(config[CONF_CHANNEL])
    var = cg.Pvariable(config[CONF_ID], rhs)
    yield output.register_output(var, config)

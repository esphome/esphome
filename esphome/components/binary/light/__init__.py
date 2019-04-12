import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import CONF_RED, CONF_OUTPUT_ID, CONF_OUTPUT
from .. import binary_ns

BinaryLightOutput = binary_ns.class_('BinaryLightOutput', light.LightOutput)

PLATFORM_SCHEMA = cv.nameable(light.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_variable_id(BinaryLightOutput),
    cv.Required(CONF_OUTPUT): cv.use_variable_id(output.BinaryOutput),
}).extend(light.BINARY_LIGHT_SCHEMA))


def to_code(config):
    out = yield cg.get_variable(config[CONF_RED])
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], out)
    yield light.register_light(var, config)

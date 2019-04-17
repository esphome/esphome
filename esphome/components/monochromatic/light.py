import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import CONF_OUTPUT_ID, CONF_OUTPUT

monochromatic_ns = cg.esphome_ns.namespace('monochromatic')
MonochromaticLightOutput = monochromatic_ns.class_('MonochromaticLightOutput', light.LightOutput)

CONFIG_SCHEMA = cv.nameable(light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_variable_id(MonochromaticLightOutput),
    cv.Required(CONF_OUTPUT): cv.use_variable_id(output.FloatOutput),
}))


def to_code(config):
    out = yield cg.get_variable(config[CONF_OUTPUT])
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], out)
    yield light.register_light(var, config)

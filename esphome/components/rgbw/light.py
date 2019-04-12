import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import CONF_BLUE, CONF_GREEN, CONF_RED, CONF_OUTPUT_ID, CONF_WHITE

rgbw_ns = cg.esphome_ns.namespace('rgbw')
RGBWLightOutput = rgbw_ns.class_('RGBWLightOutput', light.LightOutput)

PLATFORM_SCHEMA = cv.nameable(light.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_variable_id(RGBWLightOutput),
    cv.Required(CONF_RED): cv.use_variable_id(output.FloatOutput),
    cv.Required(CONF_GREEN): cv.use_variable_id(output.FloatOutput),
    cv.Required(CONF_BLUE): cv.use_variable_id(output.FloatOutput),
    cv.Required(CONF_WHITE): cv.use_variable_id(output.FloatOutput),
}).extend(light.RGB_LIGHT_SCHEMA))


def to_code(config):
    red = yield cg.get_variable(config[CONF_RED])
    green = yield cg.get_variable(config[CONF_GREEN])
    blue = yield cg.get_variable(config[CONF_BLUE])
    white = yield cg.get_variable(config[CONF_WHITE])
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], red, green, blue, white)
    yield light.register_light(var, config)

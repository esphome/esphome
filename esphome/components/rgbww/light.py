import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import CONF_BLUE, CONF_GREEN, CONF_RED, CONF_OUTPUT_ID, CONF_COLD_WHITE, \
    CONF_WARM_WHITE, CONF_COLD_WHITE_COLOR_TEMPERATURE, \
    CONF_WARM_WHITE_COLOR_TEMPERATURE

rgbww_ns = cg.esphome_ns.namespace('rgbww')
RGBWWLightOutput = rgbww_ns.class_('RGBWWLightOutput', light.LightOutput)

CONFIG_SCHEMA = cv.nameable(light.RGB_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_variable_id(RGBWWLightOutput),
    cv.Required(CONF_RED): cv.use_variable_id(output.FloatOutput),
    cv.Required(CONF_GREEN): cv.use_variable_id(output.FloatOutput),
    cv.Required(CONF_BLUE): cv.use_variable_id(output.FloatOutput),
    cv.Required(CONF_COLD_WHITE): cv.use_variable_id(output.FloatOutput),
    cv.Required(CONF_WARM_WHITE): cv.use_variable_id(output.FloatOutput),
    cv.Required(CONF_COLD_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
    cv.Required(CONF_WARM_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
}))


def to_code(config):
    red = yield cg.get_variable(config[CONF_RED])
    green = yield cg.get_variable(config[CONF_GREEN])
    blue = yield cg.get_variable(config[CONF_BLUE])
    cwhite = yield cg.get_variable(config[CONF_COLD_WHITE])
    wwhite = yield cg.get_variable(config[CONF_WARM_WHITE])
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], red, green, blue, cwhite, wwhite,
                           config[CONF_COLD_WHITE_COLOR_TEMPERATURE],
                           config[CONF_WARM_WHITE_COLOR_TEMPERATURE])
    yield light.register_light(var, config)

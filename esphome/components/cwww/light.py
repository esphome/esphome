import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import CONF_OUTPUT_ID, CONF_COLD_WHITE, CONF_WARM_WHITE, \
    CONF_COLD_WHITE_COLOR_TEMPERATURE, CONF_WARM_WHITE_COLOR_TEMPERATURE

cwww_ns = cg.esphome_ns.namespace('cwww')
CWWWLightOutput = cwww_ns.class_('CWWWLightOutput', light.LightOutput)

PLATFORM_SCHEMA = cv.nameable(light.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_variable_id(CWWWLightOutput),
    cv.Required(CONF_COLD_WHITE): cv.use_variable_id(output.FloatOutput),
    cv.Required(CONF_WARM_WHITE): cv.use_variable_id(output.FloatOutput),
    cv.Required(CONF_COLD_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
    cv.Required(CONF_WARM_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
}).extend(light.RGB_LIGHT_SCHEMA))


def to_code(config):
    cwhite = yield cg.get_variable(config[CONF_COLD_WHITE])
    wwhite = yield cg.get_variable(config[CONF_WARM_WHITE])
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], cwhite, wwhite,
                           config[CONF_COLD_WHITE_COLOR_TEMPERATURE],
                           config[CONF_WARM_WHITE_COLOR_TEMPERATURE])
    yield light.register_light(var, config)

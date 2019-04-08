import voluptuous as vol

from esphome.components import light, output
import esphome.config_validation as cv
from esphome.const import CONF_BLUE, CONF_COLD_WHITE, CONF_COLD_WHITE_COLOR_TEMPERATURE, \
    CONF_GREEN, CONF_MAKE_ID, CONF_NAME, CONF_RED, CONF_WARM_WHITE, \
    CONF_WARM_WHITE_COLOR_TEMPERATURE
from esphome.cpp_generator import get_variable, variable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App


def validate_cold_white_colder(value):
    cw = value[CONF_COLD_WHITE_COLOR_TEMPERATURE]
    ww = value[CONF_WARM_WHITE_COLOR_TEMPERATURE]
    if cw > ww:
        raise vol.Invalid("Cold white color temperature cannot be higher than warm white")
    if cw == ww:
        raise vol.Invalid("Cold white color temperature cannot be the same as warm white")
    return value


PLATFORM_SCHEMA = cv.nameable(light.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(light.MakeLight),
    vol.Required(CONF_RED): cv.use_variable_id(output.FloatOutput),
    vol.Required(CONF_GREEN): cv.use_variable_id(output.FloatOutput),
    vol.Required(CONF_BLUE): cv.use_variable_id(output.FloatOutput),
    vol.Required(CONF_COLD_WHITE): cv.use_variable_id(output.FloatOutput),
    vol.Required(CONF_WARM_WHITE): cv.use_variable_id(output.FloatOutput),
    vol.Required(CONF_COLD_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
    vol.Required(CONF_WARM_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
}).extend(light.RGB_LIGHT_SCHEMA.schema).extend(cv.COMPONENT_SCHEMA.schema),
                              validate_cold_white_colder)


def to_code(config):
    for red in get_variable(config[CONF_RED]):
        yield
    for green in get_variable(config[CONF_GREEN]):
        yield
    for blue in get_variable(config[CONF_BLUE]):
        yield
    for cold_white in get_variable(config[CONF_COLD_WHITE]):
        yield
    for warm_white in get_variable(config[CONF_WARM_WHITE]):
        yield
    rhs = App.make_rgbww_light(config[CONF_NAME], config[CONF_COLD_WHITE_COLOR_TEMPERATURE],
                               config[CONF_WARM_WHITE_COLOR_TEMPERATURE],
                               red, green, blue, cold_white, warm_white)
    light_struct = variable(config[CONF_MAKE_ID], rhs)
    light.setup_light(light_struct.Pstate, light_struct.Poutput, config)
    setup_component(light_struct.Pstate, config)

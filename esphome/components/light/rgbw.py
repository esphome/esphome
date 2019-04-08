import voluptuous as vol

from esphome.components import light, output
import esphome.config_validation as cv
from esphome.const import CONF_BLUE, CONF_GREEN, CONF_MAKE_ID, CONF_NAME, CONF_RED, CONF_WHITE
from esphome.cpp_generator import get_variable, variable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App

PLATFORM_SCHEMA = cv.nameable(light.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(light.MakeLight),
    vol.Required(CONF_RED): cv.use_variable_id(output.FloatOutput),
    vol.Required(CONF_GREEN): cv.use_variable_id(output.FloatOutput),
    vol.Required(CONF_BLUE): cv.use_variable_id(output.FloatOutput),
    vol.Required(CONF_WHITE): cv.use_variable_id(output.FloatOutput),
}).extend(light.RGB_LIGHT_SCHEMA.schema).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for red in get_variable(config[CONF_RED]):
        yield
    for green in get_variable(config[CONF_GREEN]):
        yield
    for blue in get_variable(config[CONF_BLUE]):
        yield
    for white in get_variable(config[CONF_WHITE]):
        yield
    rhs = App.make_rgbw_light(config[CONF_NAME], red, green, blue, white)
    light_struct = variable(config[CONF_MAKE_ID], rhs)
    light.setup_light(light_struct.Pstate, light_struct.Poutput, config)
    setup_component(light_struct.Pstate, config)

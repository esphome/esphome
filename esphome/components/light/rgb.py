import voluptuous as vol

from esphome.components import light, output
import esphome.config_validation as cv
from esphome.const import CONF_BLUE, CONF_DEFAULT_TRANSITION_LENGTH, CONF_EFFECTS, \
    CONF_GAMMA_CORRECT, CONF_GREEN, CONF_MAKE_ID, CONF_NAME, CONF_RED
from esphome.cpp_generator import get_variable, variable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App

PLATFORM_SCHEMA = cv.nameable(light.LIGHT_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(light.MakeLight),
    vol.Required(CONF_RED): cv.use_variable_id(output.FloatOutput),
    vol.Required(CONF_GREEN): cv.use_variable_id(output.FloatOutput),
    vol.Required(CONF_BLUE): cv.use_variable_id(output.FloatOutput),
    vol.Optional(CONF_GAMMA_CORRECT): cv.positive_float,
    vol.Optional(CONF_DEFAULT_TRANSITION_LENGTH): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_EFFECTS): light.validate_effects(light.RGB_EFFECTS),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for red in get_variable(config[CONF_RED]):
        yield
    for green in get_variable(config[CONF_GREEN]):
        yield
    for blue in get_variable(config[CONF_BLUE]):
        yield
    rhs = App.make_rgb_light(config[CONF_NAME], red, green, blue)
    light_struct = variable(config[CONF_MAKE_ID], rhs)
    light.setup_light(light_struct.Pstate, config)
    setup_component(light_struct.Pstate, config)


def to_hass_config(data, config):
    return light.core_to_hass_config(data, config, brightness=True, rgb=True, color_temp=False,
                                     white_value=False)

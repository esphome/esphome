import voluptuous as vol

from esphome.components import light, output
from esphome.components.light.rgbww import validate_cold_white_colder, \
    validate_color_temperature
import esphome.config_validation as cv
from esphome.const import CONF_COLD_WHITE, CONF_COLD_WHITE_COLOR_TEMPERATURE, \
    CONF_DEFAULT_TRANSITION_LENGTH, CONF_EFFECTS, CONF_GAMMA_CORRECT, CONF_MAKE_ID, \
    CONF_NAME, CONF_WARM_WHITE, CONF_WARM_WHITE_COLOR_TEMPERATURE
from esphome.cpp_generator import get_variable, variable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App

PLATFORM_SCHEMA = cv.nameable(light.LIGHT_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(light.MakeLight),
    vol.Required(CONF_COLD_WHITE): cv.use_variable_id(output.FloatOutput),
    vol.Required(CONF_WARM_WHITE): cv.use_variable_id(output.FloatOutput),
    vol.Required(CONF_COLD_WHITE_COLOR_TEMPERATURE): validate_color_temperature,
    vol.Required(CONF_WARM_WHITE_COLOR_TEMPERATURE): validate_color_temperature,

    vol.Optional(CONF_GAMMA_CORRECT): cv.positive_float,
    vol.Optional(CONF_DEFAULT_TRANSITION_LENGTH): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_EFFECTS): light.validate_effects(light.MONOCHROMATIC_EFFECTS),
}).extend(cv.COMPONENT_SCHEMA.schema), validate_cold_white_colder)


def to_code(config):
    for cold_white in get_variable(config[CONF_COLD_WHITE]):
        yield
    for warm_white in get_variable(config[CONF_WARM_WHITE]):
        yield
    rhs = App.make_cwww_light(config[CONF_NAME], config[CONF_COLD_WHITE_COLOR_TEMPERATURE],
                              config[CONF_WARM_WHITE_COLOR_TEMPERATURE],
                              cold_white, warm_white)
    light_struct = variable(config[CONF_MAKE_ID], rhs)
    light.setup_light(light_struct.Pstate, config)
    setup_component(light_struct.Pstate, config)


def to_hass_config(data, config):
    return light.core_to_hass_config(data, config, brightness=True, rgb=False, color_temp=True,
                                     white_value=False)

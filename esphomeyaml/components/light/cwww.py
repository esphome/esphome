import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import light
from esphomeyaml.components.light.rgbww import validate_cold_white_colder, \
    validate_color_temperature
from esphomeyaml.const import CONF_COLD_WHITE, CONF_COLD_WHITE_COLOR_TEMPERATURE, \
    CONF_DEFAULT_TRANSITION_LENGTH, CONF_EFFECTS, CONF_GAMMA_CORRECT, CONF_MAKE_ID, \
    CONF_NAME, CONF_WARM_WHITE, CONF_WARM_WHITE_COLOR_TEMPERATURE
from esphomeyaml.helpers import App, get_variable, variable

PLATFORM_SCHEMA = cv.nameable(light.LIGHT_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(light.MakeLight),
    vol.Required(CONF_COLD_WHITE): cv.use_variable_id(None),
    vol.Required(CONF_WARM_WHITE): cv.use_variable_id(None),
    vol.Required(CONF_COLD_WHITE_COLOR_TEMPERATURE): validate_color_temperature,
    vol.Required(CONF_WARM_WHITE_COLOR_TEMPERATURE): validate_color_temperature,

    vol.Optional(CONF_GAMMA_CORRECT): cv.positive_float,
    vol.Optional(CONF_DEFAULT_TRANSITION_LENGTH): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_EFFECTS): light.validate_effects(light.MONOCHROMATIC_EFFECTS),
}), validate_cold_white_colder)


def to_code(config):
    for cold_white in get_variable(config[CONF_COLD_WHITE]):
        yield
    for warm_white in get_variable(config[CONF_WARM_WHITE]):
        yield
    rhs = App.make_cwww_light(config[CONF_NAME], config[CONF_COLD_WHITE_COLOR_TEMPERATURE],
                              config[CONF_WARM_WHITE_COLOR_TEMPERATURE],
                              cold_white, warm_white)
    light_struct = variable(config[CONF_MAKE_ID], rhs)
    light.setup_light(light_struct.Pstate, light_struct.Pmqtt, config)

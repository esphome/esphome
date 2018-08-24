import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import light
from esphomeyaml.const import CONF_BLUE, CONF_COLD_WHITE, CONF_COLD_WHITE_COLOR_TEMPERATURE, \
    CONF_DEFAULT_TRANSITION_LENGTH, CONF_EFFECTS, CONF_GAMMA_CORRECT, CONF_GREEN, CONF_MAKE_ID, \
    CONF_NAME, CONF_RED, CONF_WARM_WHITE, CONF_WARM_WHITE_COLOR_TEMPERATURE
from esphomeyaml.helpers import App, get_variable, variable


def validate_color_temperature(value):
    try:
        val = cv.float_with_unit('Color Temperature', 'mireds')(value)
    except vol.Invalid:
        val = 1000000.0 / cv.float_with_unit('Color Temperature', 'K')(value)
    if val < 0:
        raise vol.Invalid("Color temperature cannot be negative")
    return val


def validate_cold_white_colder(value):
    cw = value[CONF_COLD_WHITE_COLOR_TEMPERATURE]
    ww = value[CONF_WARM_WHITE_COLOR_TEMPERATURE]
    if cw > ww:
        raise vol.Invalid("Cold white color temperature cannot be higher than warm white")
    if cw == ww:
        raise vol.Invalid("Cold white color temperature cannot be the same as warm white")
    return value


PLATFORM_SCHEMA = cv.nameable(light.LIGHT_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(light.MakeLight),
    vol.Required(CONF_RED): cv.use_variable_id(None),
    vol.Required(CONF_GREEN): cv.use_variable_id(None),
    vol.Required(CONF_BLUE): cv.use_variable_id(None),
    vol.Required(CONF_COLD_WHITE): cv.use_variable_id(None),
    vol.Required(CONF_WARM_WHITE): cv.use_variable_id(None),
    vol.Required(CONF_COLD_WHITE_COLOR_TEMPERATURE): validate_color_temperature,
    vol.Required(CONF_WARM_WHITE_COLOR_TEMPERATURE): validate_color_temperature,

    vol.Optional(CONF_GAMMA_CORRECT): cv.positive_float,
    vol.Optional(CONF_DEFAULT_TRANSITION_LENGTH): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_EFFECTS): light.validate_effects(light.RGB_EFFECTS),
}), validate_cold_white_colder)


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
    light.setup_light(light_struct.Pstate, light_struct.Pmqtt, config)

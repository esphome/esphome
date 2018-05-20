import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import light
from esphomeyaml.const import CONF_BLUE, CONF_DEFAULT_TRANSITION_LENGTH, CONF_GAMMA_CORRECT, \
    CONF_GREEN, CONF_MAKE_ID, CONF_NAME, CONF_RED, CONF_WHITE
from esphomeyaml.helpers import App, get_variable, variable

PLATFORM_SCHEMA = light.PLATFORM_SCHEMA.extend({
    cv.GenerateID('rgbw_light', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_RED): cv.variable_id,
    vol.Required(CONF_GREEN): cv.variable_id,
    vol.Required(CONF_BLUE): cv.variable_id,
    vol.Required(CONF_WHITE): cv.variable_id,
    vol.Optional(CONF_GAMMA_CORRECT): cv.positive_float,
    vol.Optional(CONF_DEFAULT_TRANSITION_LENGTH): cv.positive_time_period_milliseconds,
}).extend(light.LIGHT_SCHEMA.schema)


def to_code(config):
    red = get_variable(config[CONF_RED])
    green = get_variable(config[CONF_GREEN])
    blue = get_variable(config[CONF_BLUE])
    white = get_variable(config[CONF_WHITE])
    rhs = App.make_rgbw_light(config[CONF_NAME], red, green, blue, white)
    light_struct = variable(light.MakeLight, config[CONF_MAKE_ID], rhs)
    light.setup_light(light_struct.Pstate, light_struct.Pmqtt, config)

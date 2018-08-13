import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import light
from esphomeyaml.const import CONF_DEFAULT_TRANSITION_LENGTH, CONF_GAMMA_CORRECT, CONF_MAKE_ID, \
    CONF_NAME, CONF_OUTPUT, CONF_EFFECTS
from esphomeyaml.helpers import App, get_variable, variable

PLATFORM_SCHEMA = cv.nameable(light.LIGHT_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(light.MakeLight),
    vol.Required(CONF_OUTPUT): cv.use_variable_id(None),
    vol.Optional(CONF_GAMMA_CORRECT): cv.positive_float,
    vol.Optional(CONF_DEFAULT_TRANSITION_LENGTH): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_EFFECTS): light.validate_effects(light.MONOCHROMATIC_EFFECTS),
}))


def to_code(config):
    output = None
    for output in get_variable(config[CONF_OUTPUT]):
        yield
    rhs = App.make_monochromatic_light(config[CONF_NAME], output)
    light_struct = variable(config[CONF_MAKE_ID], rhs)
    light.setup_light(light_struct.Pstate, light_struct.Pmqtt, config)

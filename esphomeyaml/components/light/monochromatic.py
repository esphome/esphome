import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import light
from esphomeyaml.const import CONF_DEFAULT_TRANSITION_LENGTH, CONF_GAMMA_CORRECT, CONF_ID, \
    CONF_NAME, CONF_OUTPUT
from esphomeyaml.helpers import App, add, get_variable, variable, setup_mqtt_component

PLATFORM_SCHEMA = light.PLATFORM_SCHEMA.extend({
    cv.GenerateID('monochromatic_light'): cv.register_variable_id,
    vol.Required(CONF_OUTPUT): cv.variable_id,
    vol.Optional(CONF_GAMMA_CORRECT): cv.positive_float,
    vol.Optional(CONF_DEFAULT_TRANSITION_LENGTH): cv.positive_time_period,
})


def to_code(config):
    output = get_variable(config[CONF_OUTPUT])
    rhs = App.make_monochromatic_light(config[CONF_NAME], output)
    light_struct = variable('Application::MakeLight', config[CONF_ID], rhs)
    if CONF_GAMMA_CORRECT in config:
        add(light_struct.Poutput.set_gamma_correct(config[CONF_GAMMA_CORRECT]))
    setup_mqtt_component(light_struct.Pmqtt, config)
    light.setup_light_component(light_struct.Pstate, config)


import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import light
from esphomeyaml.const import CONF_ID, CONF_NAME, CONF_OUTPUT
from esphomeyaml.helpers import App, get_variable, variable, setup_mqtt_component

PLATFORM_SCHEMA = light.PLATFORM_SCHEMA.extend({
    cv.GenerateID('binary_light'): cv.register_variable_id,
    vol.Required(CONF_OUTPUT): cv.variable_id,
})


def to_code(config):
    output = get_variable(config[CONF_OUTPUT])
    rhs = App.make_binary_light(config[CONF_NAME], output)
    light_struct = variable('Application::MakeLight', config[CONF_ID], rhs)
    setup_mqtt_component(light_struct.Pmqtt, config)
    light.setup_light_component(light_struct.Pstate, config)

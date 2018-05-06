import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_ID, CONF_NAME, CONF_OUTPUT
from esphomeyaml.helpers import App, get_variable, variable

PLATFORM_SCHEMA = switch.PLATFORM_SCHEMA.extend({
    cv.GenerateID('output_switch'): cv.register_variable_id,
    vol.Required(CONF_OUTPUT): cv.variable_id,
}).extend(switch.MQTT_SWITCH_SCHEMA.schema)


def to_code(config):
    output = get_variable(config[CONF_OUTPUT])
    rhs = App.make_simple_switch(config[CONF_NAME], output)
    gpio = variable('Application::MakeSimpleSwitch', config[CONF_ID], rhs)
    switch.setup_switch(gpio.Pswitch_, config)
    switch.setup_mqtt_switch(gpio.Pmqtt, config)


BUILD_FLAGS = '-DUSE_SIMPLE_SWITCH'

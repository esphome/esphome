import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_OUTPUT
from esphomeyaml.helpers import App, Application, get_variable, variable

PLATFORM_SCHEMA = switch.PLATFORM_SCHEMA.extend({
    cv.GenerateID('output_switch', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_OUTPUT): cv.variable_id,
}).extend(switch.SWITCH_SCHEMA.schema)

MakeSimpleSwitch = Application.MakeSimpleSwitch


def to_code(config):
    output = get_variable(config[CONF_OUTPUT])
    rhs = App.make_simple_switch(config[CONF_NAME], output)
    gpio = variable(MakeSimpleSwitch, config[CONF_MAKE_ID], rhs)
    switch.setup_switch(gpio.Pswitch_, gpio.Pmqtt, config)


BUILD_FLAGS = '-DUSE_SIMPLE_SWITCH'

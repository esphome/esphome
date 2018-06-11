import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_OUTPUT
from esphomeyaml.helpers import App, Application, get_variable, variable

MakeSimpleSwitch = Application.MakeSimpleSwitch

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeSimpleSwitch),
    vol.Required(CONF_OUTPUT): cv.use_variable_id(None),
}))


def to_code(config):
    output = None
    for output in get_variable(config[CONF_OUTPUT]):
        yield
    rhs = App.make_simple_switch(config[CONF_NAME], output)
    gpio = variable(config[CONF_MAKE_ID], rhs)
    switch.setup_switch(gpio.Pswitch_, gpio.Pmqtt, config)


BUILD_FLAGS = '-DUSE_SIMPLE_SWITCH'

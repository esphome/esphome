import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_INVERTED, CONF_MAKE_ID, CONF_NAME
from esphomeyaml.helpers import App, Application, variable

MakeShutdownSwitch = Application.MakeShutdownSwitch

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeShutdownSwitch),
    vol.Optional(CONF_INVERTED): cv.invalid("Shutdown switches do not support inverted mode!"),
}))


def to_code(config):
    rhs = App.make_shutdown_switch(config[CONF_NAME])
    shutdown = variable(config[CONF_MAKE_ID], rhs)
    switch.setup_switch(shutdown.Pshutdown, shutdown.Pmqtt, config)


BUILD_FLAGS = '-DUSE_SHUTDOWN_SWITCH'

import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_INVERTED, CONF_MAKE_ID, CONF_NAME
from esphomeyaml.helpers import App, Application, variable

PLATFORM_SCHEMA = switch.PLATFORM_SCHEMA.extend({
    cv.GenerateID('shutdown_switch', CONF_MAKE_ID): cv.register_variable_id,
    vol.Optional(CONF_INVERTED): cv.invalid("Shutdown switches do not support inverted mode!"),
}).extend(switch.SWITCH_SCHEMA.schema)

MakeShutdownSwitch = Application.MakeShutdownSwitch


def to_code(config):
    rhs = App.make_shutdown_switch(config[CONF_NAME])
    shutdown = variable(MakeShutdownSwitch, config[CONF_MAKE_ID], rhs)
    switch.setup_switch(shutdown.Pshutdown, shutdown.Pmqtt, config)


BUILD_FLAGS = '-DUSE_SHUTDOWN_SWITCH'

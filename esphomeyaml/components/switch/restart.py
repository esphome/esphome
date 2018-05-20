import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_INVERTED, CONF_MAKE_ID, CONF_NAME
from esphomeyaml.helpers import App, Application, variable

PLATFORM_SCHEMA = switch.PLATFORM_SCHEMA.extend({
    cv.GenerateID('restart_switch', CONF_MAKE_ID): cv.register_variable_id,
    vol.Optional(CONF_INVERTED): cv.invalid("Restart switches do not support inverted mode!"),
}).extend(switch.SWITCH_SCHEMA.schema)

MakeRestartSwitch = Application.MakeRestartSwitch


def to_code(config):
    rhs = App.make_restart_switch(config[CONF_NAME])
    restart = variable(MakeRestartSwitch, config[CONF_MAKE_ID], rhs)
    switch.setup_switch(restart.Prestart, restart.Pmqtt, config)


BUILD_FLAGS = '-DUSE_RESTART_SWITCH'

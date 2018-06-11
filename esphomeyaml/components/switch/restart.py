import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_INVERTED, CONF_MAKE_ID, CONF_NAME
from esphomeyaml.helpers import App, Application, variable

MakeRestartSwitch = Application.MakeRestartSwitch

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeRestartSwitch),
    vol.Optional(CONF_INVERTED): cv.invalid("Restart switches do not support inverted mode!"),
}))


def to_code(config):
    rhs = App.make_restart_switch(config[CONF_NAME])
    restart = variable(config[CONF_MAKE_ID], rhs)
    switch.setup_switch(restart.Prestart, restart.Pmqtt, config)


BUILD_FLAGS = '-DUSE_RESTART_SWITCH'

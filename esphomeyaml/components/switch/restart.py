import voluptuous as vol

from esphomeyaml.components import switch
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_INVERTED, CONF_MAKE_ID, CONF_NAME
from esphomeyaml.cpp_generator import variable
from esphomeyaml.cpp_types import App, Application

MakeRestartSwitch = Application.struct('MakeRestartSwitch')
RestartSwitch = switch.switch_ns.class_('RestartSwitch', switch.Switch)

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(RestartSwitch),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeRestartSwitch),
    vol.Optional(CONF_INVERTED): cv.invalid("Restart switches do not support inverted mode!"),
}))


def to_code(config):
    rhs = App.make_restart_switch(config[CONF_NAME])
    restart = variable(config[CONF_MAKE_ID], rhs)
    switch.setup_switch(restart.Prestart, restart.Pmqtt, config)


BUILD_FLAGS = '-DUSE_RESTART_SWITCH'


def to_hass_config(data, config):
    return switch.core_to_hass_config(data, config)

import voluptuous as vol

from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INVERTED, CONF_NAME
from esphome.cpp_generator import Pvariable
from esphome.cpp_types import App

RestartSwitch = switch.switch_ns.class_('RestartSwitch', switch.Switch)

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(RestartSwitch),
    vol.Optional(CONF_INVERTED): cv.invalid("Restart switches do not support inverted mode!"),
}))


def to_code(config):
    rhs = App.make_restart_switch(config[CONF_NAME])
    restart = Pvariable(config[CONF_ID], rhs)
    switch.setup_switch(restart, config)


BUILD_FLAGS = '-DUSE_RESTART_SWITCH'


def to_hass_config(data, config):
    return switch.core_to_hass_config(data, config)

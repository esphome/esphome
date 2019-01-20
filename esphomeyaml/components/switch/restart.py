import voluptuous as vol

from esphomeyaml.components import switch
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_INVERTED, CONF_MAKE_ID, CONF_NAME, CONF_ID
from esphomeyaml.cpp_generator import variable, Pvariable
from esphomeyaml.cpp_types import App, Application

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

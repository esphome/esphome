import voluptuous as vol

from esphome import pins
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERLOCK, CONF_NAME, CONF_PIN, CONF_RESTORE_MODE
from esphome.cpp_generator import Pvariable, add, get_variable
from esphome.cpp_helpers import gpio_output_pin_expression, setup_component
from esphome.cpp_types import App, Component

GPIOSwitch = switch.switch_ns.class_('GPIOSwitch', switch.Switch, Component)
GPIOSwitchRestoreMode = switch.switch_ns.enum('GPIOSwitchRestoreMode')

RESTORE_MODES = {
    'RESTORE_DEFAULT_OFF': GPIOSwitchRestoreMode.GPIO_SWITCH_RESTORE_DEFAULT_OFF,
    'RESTORE_DEFAULT_ON': GPIOSwitchRestoreMode.GPIO_SWITCH_RESTORE_DEFAULT_ON,
    'ALWAYS_OFF': GPIOSwitchRestoreMode.GPIO_SWITCH_ALWAYS_OFF,
    'ALWAYS_ON': GPIOSwitchRestoreMode.GPIO_SWITCH_ALWAYS_ON,
}

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(GPIOSwitch),
    vol.Required(CONF_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_RESTORE_MODE): cv.one_of(*RESTORE_MODES, upper=True, space='_'),
    vol.Optional(CONF_INTERLOCK): cv.ensure_list(cv.use_variable_id(switch.Switch)),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for pin in gpio_output_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_gpio_switch(config[CONF_NAME], pin)
    gpio = Pvariable(config[CONF_ID], rhs)

    if CONF_RESTORE_MODE in config:
        add(gpio.set_restore_mode(RESTORE_MODES[config[CONF_RESTORE_MODE]]))

    if CONF_INTERLOCK in config:
        interlock = []
        for it in config[CONF_INTERLOCK]:
            for lock in get_variable(it):
                yield
            interlock.append(lock)
        add(gpio.set_interlock(interlock))

    switch.setup_switch(gpio, config)
    setup_component(gpio, config)


BUILD_FLAGS = '-DUSE_GPIO_SWITCH'


def to_hass_config(data, config):
    return switch.core_to_hass_config(data, config)

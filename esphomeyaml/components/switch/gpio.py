import voluptuous as vol

from esphomeyaml import pins
from esphomeyaml.components import switch
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_PIN, CONF_RESTORE_MODE
from esphomeyaml.cpp_generator import add, variable
from esphomeyaml.cpp_helpers import gpio_output_pin_expression, setup_component
from esphomeyaml.cpp_types import App, Application, Component

MakeGPIOSwitch = Application.struct('MakeGPIOSwitch')
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
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeGPIOSwitch),
    vol.Required(CONF_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_RESTORE_MODE): cv.one_of(*RESTORE_MODES, upper=True, space='_'),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for pin in gpio_output_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_gpio_switch(config[CONF_NAME], pin)
    make = variable(config[CONF_MAKE_ID], rhs)
    gpio = make.Pswitch_

    if CONF_RESTORE_MODE in config:
        add(gpio.set_restore_mode(RESTORE_MODES[config[CONF_RESTORE_MODE]]))

    switch.setup_switch(gpio, make.Pmqtt, config)
    setup_component(gpio, config)


BUILD_FLAGS = '-DUSE_GPIO_SWITCH'


def to_hass_config(data, config):
    return switch.core_to_hass_config(data, config)

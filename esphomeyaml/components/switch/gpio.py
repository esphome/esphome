import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_PIN
from esphomeyaml.helpers import App, Application, gpio_output_pin_expression, variable

MakeGPIOSwitch = Application.MakeGPIOSwitch

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeGPIOSwitch),
    vol.Required(CONF_PIN): pins.gpio_output_pin_schema,
}))


def to_code(config):
    pin = None
    for pin in gpio_output_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_gpio_switch(config[CONF_NAME], pin)
    gpio = variable(config[CONF_MAKE_ID], rhs)
    switch.setup_switch(gpio.Pswitch_, gpio.Pmqtt, config)


BUILD_FLAGS = '-DUSE_GPIO_SWITCH'

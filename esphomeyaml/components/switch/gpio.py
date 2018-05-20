import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_PIN
from esphomeyaml.helpers import App, Application, gpio_output_pin_expression, variable

PLATFORM_SCHEMA = switch.PLATFORM_SCHEMA.extend({
    cv.GenerateID('gpio_switch', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_PIN): pins.GPIO_OUTPUT_PIN_SCHEMA,
}).extend(switch.SWITCH_SCHEMA.schema)

MakeGPIOSwitch = Application.MakeGPIOSwitch


def to_code(config):
    rhs = App.make_gpio_switch(config[CONF_NAME], gpio_output_pin_expression(config[CONF_PIN]))
    gpio = variable(MakeGPIOSwitch, config[CONF_MAKE_ID], rhs)
    switch.setup_switch(gpio.Pswitch_, gpio.Pmqtt, config)


BUILD_FLAGS = '-DUSE_GPIO_SWITCH'

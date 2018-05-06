import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_ID, CONF_NAME, CONF_PIN
from esphomeyaml.helpers import App, exp_gpio_output_pin, variable

PLATFORM_SCHEMA = switch.PLATFORM_SCHEMA.extend({
    cv.GenerateID('gpio_switch'): cv.register_variable_id,
    vol.Required(CONF_PIN): pins.GPIO_OUTPUT_PIN_SCHEMA,
}).extend(switch.MQTT_SWITCH_SCHEMA.schema)


def to_code(config):
    rhs = App.make_gpio_switch(config[CONF_NAME], exp_gpio_output_pin(config[CONF_PIN]))
    gpio = variable('Application::MakeGPIOSwitch', config[CONF_ID], rhs)
    switch.setup_switch(gpio.Pswitch_, config)
    switch.setup_mqtt_switch(gpio.Pmqtt, config)


BUILD_FLAGS = '-DUSE_GPIO_SWITCH'

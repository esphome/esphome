import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import binary_sensor
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_PIN
from esphomeyaml.helpers import App, gpio_input_pin_expression, variable, Application

PLATFORM_SCHEMA = binary_sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('gpio_binary_sensor', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_PIN): pins.GPIO_INPUT_PIN_SCHEMA
}).extend(binary_sensor.BINARY_SENSOR_SCHEMA.schema)

MakeGPIOBinarySensor = Application.MakeGPIOBinarySensor


def to_code(config):
    rhs = App.make_gpio_binary_sensor(config[CONF_NAME],
                                      gpio_input_pin_expression(config[CONF_PIN]))
    gpio = variable(MakeGPIOBinarySensor, config[CONF_MAKE_ID], rhs)
    binary_sensor.setup_binary_sensor(gpio.Pgpio, gpio.Pmqtt, config)


BUILD_FLAGS = '-DUSE_GPIO_BINARY_SENSOR'

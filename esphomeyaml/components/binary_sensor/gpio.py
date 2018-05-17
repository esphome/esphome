import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import binary_sensor
from esphomeyaml.const import CONF_ID, CONF_INVERTED, CONF_NAME, CONF_PIN
from esphomeyaml.helpers import App, add, variable, gpio_input_pin_expression

PLATFORM_SCHEMA = binary_sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('gpio_binary_sensor'): cv.register_variable_id,
    vol.Required(CONF_PIN): pins.GPIO_INPUT_PIN_SCHEMA
}).extend(binary_sensor.MQTT_BINARY_SENSOR_SCHEMA.schema)


def to_code(config):
    rhs = App.make_gpio_binary_sensor(config[CONF_NAME],
                                      gpio_input_pin_expression(config[CONF_PIN]))
    gpio = variable('Application::MakeGPIOBinarySensor', config[CONF_ID], rhs)
    if CONF_INVERTED in config:
        add(gpio.Pgpio.set_inverted(config[CONF_INVERTED]))
    binary_sensor.setup_binary_sensor(gpio.Pgpio, config)
    binary_sensor.setup_mqtt_binary_sensor(gpio.Pmqtt, config)


BUILD_FLAGS = '-DUSE_GPIO_BINARY_SENSOR'

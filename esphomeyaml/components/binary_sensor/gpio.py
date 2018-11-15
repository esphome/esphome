import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import binary_sensor
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_PIN
from esphomeyaml.helpers import App, gpio_input_pin_expression, variable, Application, \
    setup_component, Component

MakeGPIOBinarySensor = Application.struct('MakeGPIOBinarySensor')
GPIOBinarySensorComponent = binary_sensor.binary_sensor_ns.class_('GPIOBinarySensorComponent',
                                                                  binary_sensor.BinarySensor,
                                                                  Component)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(GPIOBinarySensorComponent),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeGPIOBinarySensor),
    vol.Required(CONF_PIN): pins.gpio_input_pin_schema
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    pin = None
    for pin in gpio_input_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_gpio_binary_sensor(config[CONF_NAME], pin)
    gpio = variable(config[CONF_MAKE_ID], rhs)
    binary_sensor.setup_binary_sensor(gpio.Pgpio, gpio.Pmqtt, config)
    setup_component(gpio.Pgpio, config)


BUILD_FLAGS = '-DUSE_GPIO_BINARY_SENSOR'


def to_hass_config(data, config):
    return binary_sensor.core_to_hass_config(data, config)

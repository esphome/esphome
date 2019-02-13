import voluptuous as vol

from esphome import pins
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_PIN
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import gpio_input_pin_expression, setup_component
from esphome.cpp_types import App, Component

GPIOBinarySensorComponent = binary_sensor.binary_sensor_ns.class_('GPIOBinarySensorComponent',
                                                                  binary_sensor.BinarySensor,
                                                                  Component)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(GPIOBinarySensorComponent),
    vol.Required(CONF_PIN): pins.gpio_input_pin_schema
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    pin = None
    for pin in gpio_input_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_gpio_binary_sensor(config[CONF_NAME], pin)
    gpio = Pvariable(config[CONF_ID], rhs)
    binary_sensor.setup_binary_sensor(gpio, config)
    setup_component(gpio, config)


BUILD_FLAGS = '-DUSE_GPIO_BINARY_SENSOR'


def to_hass_config(data, config):
    return binary_sensor.core_to_hass_config(data, config)

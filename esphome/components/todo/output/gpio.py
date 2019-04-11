import voluptuous as vol

from esphome import pins
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import gpio_output_pin_expression, register_component
from esphome.cpp_types import App, Component

GPIOBinaryOutputComponent = output.output_ns.class_('GPIOBinaryOutputComponent',
                                                    output.BinaryOutput, Component)

PLATFORM_SCHEMA = output.BINARY_OUTPUT_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_ID): cv.declare_variable_id(GPIOBinaryOutputComponent),
    vol.Required(CONF_PIN): pins.gpio_output_pin_schema,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    pin = yield gpio_output_pin_expression(config[CONF_PIN])
    rhs = App.make_gpio_output(pin)
    gpio = Pvariable(config[CONF_ID], rhs)
    output.setup_output_platform(gpio, config)
    register_component(gpio, config)


BUILD_FLAGS = '-DUSE_GPIO_OUTPUT'

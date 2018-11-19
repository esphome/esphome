import voluptuous as vol

from esphomeyaml import pins
from esphomeyaml.components import output
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_PIN
from esphomeyaml.cpp_generator import Pvariable
from esphomeyaml.cpp_helpers import gpio_output_pin_expression, setup_component
from esphomeyaml.cpp_types import App, Component

GPIOBinaryOutputComponent = output.output_ns.class_('GPIOBinaryOutputComponent',
                                                    output.BinaryOutput, Component)

PLATFORM_SCHEMA = output.BINARY_OUTPUT_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_ID): cv.declare_variable_id(GPIOBinaryOutputComponent),
    vol.Required(CONF_PIN): pins.gpio_output_pin_schema,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for pin in gpio_output_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_gpio_output(pin)
    gpio = Pvariable(config[CONF_ID], rhs)
    output.setup_output_platform(gpio, config)
    setup_component(gpio, config)


BUILD_FLAGS = '-DUSE_GPIO_OUTPUT'

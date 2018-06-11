import voluptuous as vol

from esphomeyaml import pins
import esphomeyaml.config_validation as cv
from esphomeyaml.components import output
from esphomeyaml.const import CONF_ID, CONF_PIN
from esphomeyaml.helpers import App, Pvariable, gpio_output_pin_expression

GPIOBinaryOutputComponent = output.output_ns.GPIOBinaryOutputComponent

PLATFORM_SCHEMA = output.BINARY_OUTPUT_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_ID): cv.declare_variable_id(GPIOBinaryOutputComponent),
    vol.Required(CONF_PIN): pins.gpio_output_pin_schema,
})


def to_code(config):
    pin = None
    for pin in gpio_output_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_gpio_output(pin)
    gpio = Pvariable(config[CONF_ID], rhs)
    output.setup_output_platform(gpio, config)


BUILD_FLAGS = '-DUSE_GPIO_OUTPUT'

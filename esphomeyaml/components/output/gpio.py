import voluptuous as vol

from esphomeyaml import pins
from esphomeyaml.components import output
from esphomeyaml.const import CONF_ID, CONF_PIN
from esphomeyaml.helpers import App, Pvariable, gpio_output_pin_expression

PLATFORM_SCHEMA = output.PLATFORM_SCHEMA.extend({
    vol.Required(CONF_PIN): pins.GPIO_OUTPUT_PIN_SCHEMA,
}).extend(output.BINARY_OUTPUT_SCHEMA.schema)

GPIOBinaryOutputComponent = output.output_ns.GPIOBinaryOutputComponent


def to_code(config):
    pin = gpio_output_pin_expression(config[CONF_PIN])
    rhs = App.make_gpio_output(pin)
    gpio = Pvariable(GPIOBinaryOutputComponent, config[CONF_ID], rhs)
    output.setup_output_platform(gpio, config)


BUILD_FLAGS = '-DUSE_GPIO_OUTPUT'

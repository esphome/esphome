import voluptuous as vol

from esphomeyaml import pins
from esphomeyaml.components import output
from esphomeyaml.const import CONF_ID, CONF_PIN
from esphomeyaml.helpers import App, Pvariable, exp_gpio_output_pin

PLATFORM_SCHEMA = output.PLATFORM_SCHEMA.extend({
    vol.Required(CONF_PIN): pins.GPIO_OUTPUT_PIN_SCHEMA,
})


def to_code(config):
    pin = exp_gpio_output_pin(config[CONF_PIN])
    rhs = App.make_gpio_output(pin)
    gpio = Pvariable('output::GPIOBinaryOutputComponent', config[CONF_ID], rhs)
    output.setup_output_platform(gpio, config)


BUILD_FLAGS = '-DUSE_GPIO_OUTPUT'

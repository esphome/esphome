import voluptuous as vol

from esphomeyaml import pins
from esphomeyaml.components import output
from esphomeyaml.const import CONF_ID, CONF_PIN, \
    ESP_PLATFORM_ESP8266
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.helpers import App, Pvariable, exp_gpio_output_pin, get_gpio_pin_number

ESP_PLATFORMS = [ESP_PLATFORM_ESP8266]

PLATFORM_SCHEMA = output.FLOAT_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_PIN): pins.GPIO_OUTPUT_PIN_SCHEMA,
})


def to_code(config):
    if get_gpio_pin_number(config[CONF_PIN]) >= 16:
        # Too difficult to do in config validation
        raise ESPHomeYAMLError(u"ESP8266: Only pins 0-16 support PWM.")
    pin = exp_gpio_output_pin(config[CONF_PIN])
    rhs = App.make_esp8266_pwm_output(pin)
    gpio = Pvariable('output::ESP8266PWMOutput', config[CONF_ID], rhs)
    output.setup_output_platform(gpio, config)

import voluptuous as vol

from esphomeyaml import pins
from esphomeyaml.components import output
from esphomeyaml.const import CONF_ID, CONF_PIN, ESP_PLATFORM_ESP8266
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.helpers import App, Pvariable, exp_gpio_output_pin

ESP_PLATFORMS = [ESP_PLATFORM_ESP8266]


def valid_pwm_pin(value):
    if value >= 16:
        raise ESPHomeYAMLError(u"ESP8266: Only pins 0-16 support PWM.")
    return value


PLATFORM_SCHEMA = output.FLOAT_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_PIN): vol.All(pins.GPIO_OUTPUT_PIN_SCHEMA,
                                    pins.schema_validate_number(valid_pwm_pin)),
})


def to_code(config):
    pin = exp_gpio_output_pin(config[CONF_PIN])
    rhs = App.make_esp8266_pwm_output(pin)
    gpio = Pvariable('output::ESP8266PWMOutput', config[CONF_ID], rhs)
    output.setup_output_platform(gpio, config)


BUILD_FLAGS = '-DUSE_ESP8266_PWM_OUTPUT'

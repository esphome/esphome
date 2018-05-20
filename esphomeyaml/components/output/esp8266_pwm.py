import voluptuous as vol

from esphomeyaml import pins
from esphomeyaml.components import output
from esphomeyaml.const import CONF_ID, CONF_PIN, ESP_PLATFORM_ESP8266, CONF_NUMBER
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.helpers import App, Pvariable, gpio_output_pin_expression

ESP_PLATFORMS = [ESP_PLATFORM_ESP8266]


def valid_pwm_pin(value):
    if value[CONF_NUMBER] >= 16:
        raise ESPHomeYAMLError(u"ESP8266: Only pins 0-16 support PWM.")
    return value


PLATFORM_SCHEMA = output.PLATFORM_SCHEMA.extend({
    vol.Required(CONF_PIN): vol.All(pins.GPIO_INTERNAL_OUTPUT_PIN_SCHEMA, valid_pwm_pin),
}).extend(output.FLOAT_OUTPUT_SCHEMA.schema)

ESP8266PWMOutput = output.output_ns.ESP8266PWMOutput


def to_code(config):
    pin = gpio_output_pin_expression(config[CONF_PIN])
    rhs = App.make_esp8266_pwm_output(pin)
    gpio = Pvariable(ESP8266PWMOutput, config[CONF_ID], rhs)
    output.setup_output_platform(gpio, config)


BUILD_FLAGS = '-DUSE_ESP8266_PWM_OUTPUT'

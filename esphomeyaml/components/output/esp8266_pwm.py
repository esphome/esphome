import voluptuous as vol

from esphomeyaml import pins
import esphomeyaml.config_validation as cv
from esphomeyaml.components import output
from esphomeyaml.const import CONF_ID, CONF_NUMBER, CONF_PIN, ESP_PLATFORM_ESP8266
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.helpers import App, Pvariable, gpio_output_pin_expression

ESP_PLATFORMS = [ESP_PLATFORM_ESP8266]


def valid_pwm_pin(value):
    if value[CONF_NUMBER] > 16:
        raise ESPHomeYAMLError(u"ESP8266: Only pins 0-16 support PWM.")
    return value


ESP8266PWMOutput = output.output_ns.ESP8266PWMOutput

PLATFORM_SCHEMA = output.FLOAT_OUTPUT_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_ID): cv.declare_variable_id(ESP8266PWMOutput),
    vol.Required(CONF_PIN): vol.All(pins.internal_gpio_output_pin_schema, valid_pwm_pin),
})


def to_code(config):
    pin = None
    for pin in gpio_output_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_esp8266_pwm_output(pin)
    gpio = Pvariable(config[CONF_ID], rhs)
    output.setup_output_platform(gpio, config)


BUILD_FLAGS = '-DUSE_ESP8266_PWM_OUTPUT'

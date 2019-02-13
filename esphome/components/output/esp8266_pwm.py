import voluptuous as vol

from esphome import pins
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY, CONF_ID, CONF_NUMBER, CONF_PIN, ESP_PLATFORM_ESP8266
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_output_pin_expression, setup_component
from esphome.cpp_types import App, Component

ESP_PLATFORMS = [ESP_PLATFORM_ESP8266]


def valid_pwm_pin(value):
    num = value[CONF_NUMBER]
    cv.one_of(0, 1, 2, 3, 4, 5, 9, 10, 12, 13, 14, 15, 16)(num)
    return value


ESP8266PWMOutput = output.output_ns.class_('ESP8266PWMOutput', output.FloatOutput, Component)

PLATFORM_SCHEMA = output.FLOAT_OUTPUT_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_ID): cv.declare_variable_id(ESP8266PWMOutput),
    vol.Required(CONF_PIN): vol.All(pins.internal_gpio_output_pin_schema, valid_pwm_pin),
    vol.Optional(CONF_FREQUENCY): vol.All(cv.frequency, vol.Range(min=1.0e-6)),
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for pin in gpio_output_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_esp8266_pwm_output(pin)
    gpio = Pvariable(config[CONF_ID], rhs)

    if CONF_FREQUENCY in config:
        add(gpio.set_frequency(config[CONF_FREQUENCY]))

    output.setup_output_platform(gpio, config)
    setup_component(gpio, config)


BUILD_FLAGS = '-DUSE_ESP8266_PWM_OUTPUT'

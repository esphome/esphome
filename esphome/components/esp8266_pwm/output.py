from esphome import pins
from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_FREQUENCY, CONF_ID, CONF_NUMBER, CONF_PIN, ESP_PLATFORM_ESP8266

ESP_PLATFORMS = [ESP_PLATFORM_ESP8266]


def valid_pwm_pin(value):
    num = value[CONF_NUMBER]
    cv.one_of(0, 1, 2, 3, 4, 5, 9, 10, 12, 13, 14, 15, 16)(num)
    return value


esp8266_pwm_ns = cg.esphome_ns.namespace('esp8266_pwm')
ESP8266PWM = esp8266_pwm_ns.class_('ESP8266PWM', output.FloatOutput, cg.Component)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(ESP8266PWM),
    cv.Required(CONF_PIN): cv.All(pins.internal_gpio_output_pin_schema, valid_pwm_pin),
    cv.Optional(CONF_FREQUENCY, default='1kHz'): cv.All(cv.frequency, cv.Range(min=1.0e-6)),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield output.register_output(var, config)

    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    cg.add(var.set_frequency(config[CONF_FREQUENCY]))

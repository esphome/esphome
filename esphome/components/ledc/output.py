
from esphome import pins
from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import APB_CLOCK_FREQ, CONF_BIT_DEPTH, CONF_CHANNEL, CONF_FREQUENCY, \
    CONF_ID, CONF_PIN, ESP_PLATFORM_ESP32

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]


def validate_frequency_bit_depth(obj):
    frequency = obj[CONF_FREQUENCY]
    bit_depth = obj[CONF_BIT_DEPTH]
    max_freq = APB_CLOCK_FREQ / (2**bit_depth)
    if frequency > max_freq:
        raise cv.Invalid('Maximum frequency for bit depth {} is {}Hz'.format(bit_depth, max_freq))
    return obj


ledc_ns = cg.esphome_ns.namespace('ledc')
LEDCOutput = ledc_ns.class_('LEDCOutput', output.FloatOutput, cg.Component)

CONFIG_SCHEMA = cv.All(output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(LEDCOutput),
    cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_FREQUENCY, default='1kHz'): cv.frequency,
    cv.Optional(CONF_BIT_DEPTH, default=12): cv.All(cv.int_, cv.Range(min=1, max=15)),
    cv.Optional(CONF_CHANNEL): cv.All(cv.int_, cv.Range(min=0, max=15))
}).extend(cv.COMPONENT_SCHEMA), validate_frequency_bit_depth)


def to_code(config):
    gpio = yield cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], gpio)
    yield cg.register_component(var, config)
    yield output.register_output(var, config)
    if CONF_CHANNEL in config:
        cg.add(var.set_channel(config[CONF_CHANNEL]))

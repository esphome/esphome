import math

from esphome import pins
from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_BIT_DEPTH, CONF_CHANNEL, CONF_FREQUENCY, \
    CONF_ID, CONF_PIN, ESP_PLATFORM_ESP32

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]


def calc_max_frequency(bit_depth):
    return 80e6 / (2**bit_depth)


def calc_min_frequency(bit_depth):
    # LEDC_DIV_NUM_HSTIMER is 15-bit unsigned integer
    # lower 8 bits represent fractional part
    max_div_num = ((1 << 16) - 1) / 256.0
    return 80e6 / (max_div_num * (2**bit_depth))


def validate_frequency_bit_depth(obj):
    frequency = obj[CONF_FREQUENCY]
    if CONF_BIT_DEPTH not in obj:
        obj = obj.copy()
        for bit_depth in range(15, 0, -1):
            if calc_min_frequency(bit_depth) <= frequency <= calc_max_frequency(bit_depth):
                obj[CONF_BIT_DEPTH] = bit_depth
                break
        else:
            min_freq = min(calc_min_frequency(x) for x in range(1, 16))
            max_freq = max(calc_max_frequency(x) for x in range(1, 16))
            if frequency < min_freq:
                raise cv.Invalid("This frequency setting is not possible, please choose a higher "
                                 "frequency (at least {}Hz)".format(int(min_freq)))
            if frequency > max_freq:
                raise cv.Invalid("This frequency setting is not possible, please choose a lower "
                                 "frequency (at most {}Hz)".format(int(max_freq)))
            raise cv.Invalid("Invalid frequency!")

    bit_depth = obj[CONF_BIT_DEPTH]
    min_freq = calc_min_frequency(bit_depth)
    max_freq = calc_max_frequency(bit_depth)
    if frequency > max_freq:
        raise cv.Invalid('Maximum frequency for bit depth {} is {}Hz. Please decrease the '
                         'bit_depth.'.format(bit_depth, int(math.floor(max_freq))))
    if frequency < calc_min_frequency(bit_depth):
        raise cv.Invalid('Minimum frequency for bit depth {} is {}Hz. Please increase the '
                         'bit_depth.'.format(bit_depth, int(math.ceil(min_freq))))
    return obj


ledc_ns = cg.esphome_ns.namespace('ledc')
LEDCOutput = ledc_ns.class_('LEDCOutput', output.FloatOutput, cg.Component)

CONFIG_SCHEMA = cv.All(output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(LEDCOutput),
    cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_FREQUENCY, default='1kHz'): cv.frequency,
    cv.Optional(CONF_BIT_DEPTH): cv.int_range(min=1, max=15),
    cv.Optional(CONF_CHANNEL): cv.int_range(min=0, max=15),
}).extend(cv.COMPONENT_SCHEMA), validate_frequency_bit_depth)


def to_code(config):
    gpio = yield cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], gpio)
    yield cg.register_component(var, config)
    yield output.register_output(var, config)
    if CONF_CHANNEL in config:
        cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add(var.set_bit_depth(config[CONF_BIT_DEPTH]))

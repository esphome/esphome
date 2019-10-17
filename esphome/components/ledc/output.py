from esphome import pins, automation
from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_BIT_DEPTH, CONF_CHANNEL, CONF_FREQUENCY, \
    CONF_ID, CONF_PIN, ESP_PLATFORM_ESP32

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]


def calc_max_frequency(bit_depth):
    return 80e6 / (2**bit_depth)


def calc_min_frequency(bit_depth):
    max_div_num = ((2**20) - 1) / 256.0
    return 80e6 / (max_div_num * (2**bit_depth))


def validate_frequency(value):
    value = cv.frequency(value)
    min_freq = calc_min_frequency(20)
    max_freq = calc_max_frequency(1)
    if value < min_freq:
        raise cv.Invalid("This frequency setting is not possible, please choose a higher "
                         "frequency (at least {}Hz)".format(int(min_freq)))
    if value > max_freq:
        raise cv.Invalid("This frequency setting is not possible, please choose a lower "
                         "frequency (at most {}Hz)".format(int(max_freq)))
    return value


ledc_ns = cg.esphome_ns.namespace('ledc')
LEDCOutput = ledc_ns.class_('LEDCOutput', output.FloatOutput, cg.Component)
SetFrequencyAction = ledc_ns.class_('SetFrequencyAction', automation.Action)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(LEDCOutput),
    cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_FREQUENCY, default='1kHz'): cv.frequency,
    cv.Optional(CONF_CHANNEL): cv.int_range(min=0, max=15),

    cv.Optional(CONF_BIT_DEPTH): cv.invalid("The bit_depth option has been removed in v1.14, the "
                                            "best bit depth is now automatically calculated."),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    gpio = yield cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], gpio)
    yield cg.register_component(var, config)
    yield output.register_output(var, config)
    if CONF_CHANNEL in config:
        cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))


@automation.register_action('output.ledc.set_frequency', SetFrequencyAction, cv.Schema({
    cv.Required(CONF_ID): cv.use_id(LEDCOutput),
    cv.Required(CONF_FREQUENCY): cv.templatable(validate_frequency),
}))
def ledc_set_frequency_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_FREQUENCY], args, float)
    cg.add(var.set_frequency(template_))
    yield var

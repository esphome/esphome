import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import output
from esphomeyaml.const import APB_CLOCK_FREQ, CONF_BIT_DEPTH, CONF_CHANNEL, CONF_FREQUENCY, \
    CONF_ID, CONF_PIN, ESP_PLATFORM_ESP32
from esphomeyaml.helpers import App, Pvariable, add

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]


def validate_frequency_bit_depth(obj):
    frequency = obj.get(CONF_FREQUENCY, 1000)
    bit_depth = obj.get(CONF_BIT_DEPTH, 12)
    max_freq = APB_CLOCK_FREQ / (2**bit_depth)
    if frequency > max_freq:
        raise vol.Invalid('Maximum frequency for bit depth {} is {}Hz'.format(bit_depth, max_freq))
    return obj


LEDCOutputComponent = output.output_ns.LEDCOutputComponent

PLATFORM_SCHEMA = vol.All(output.FLOAT_OUTPUT_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_ID): cv.declare_variable_id(LEDCOutputComponent),
    vol.Required(CONF_PIN): pins.output_pin,
    vol.Optional(CONF_FREQUENCY): cv.frequency,
    vol.Optional(CONF_BIT_DEPTH): vol.All(vol.Coerce(int), vol.Range(min=1, max=15)),
    vol.Optional(CONF_CHANNEL): vol.All(vol.Coerce(int), vol.Range(min=0, max=15))
}), validate_frequency_bit_depth)


def to_code(config):
    frequency = config.get(CONF_FREQUENCY)
    if frequency is None and CONF_BIT_DEPTH in config:
        frequency = 1000
    rhs = App.make_ledc_output(config[CONF_PIN], frequency, config.get(CONF_BIT_DEPTH))
    ledc = Pvariable(config[CONF_ID], rhs)
    if CONF_CHANNEL in config:
        add(ledc.set_channel(config[CONF_CHANNEL]))
    output.setup_output_platform(ledc, config)


BUILD_FLAGS = '-DUSE_LEDC_OUTPUT'

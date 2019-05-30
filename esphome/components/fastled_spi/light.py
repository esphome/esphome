import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import fastled_base
from esphome.const import CONF_CHIPSET, CONF_CLOCK_PIN, CONF_DATA_PIN, CONF_NUM_LEDS, CONF_RGB_ORDER

AUTO_LOAD = ['fastled_base']

CHIPSETS = [
    'LPD8806',
    'WS2801',
    'WS2803',
    'SM16716',
    'P9813',
    'APA102',
    'SK9822',
    'DOTSTAR',
]

CONFIG_SCHEMA = fastled_base.BASE_SCHEMA.extend({
    cv.Required(CONF_CHIPSET): cv.one_of(*CHIPSETS, upper=True),
    cv.Required(CONF_DATA_PIN): pins.output_pin,
    cv.Required(CONF_CLOCK_PIN): pins.output_pin,
})


def to_code(config):
    var = yield fastled_base.new_fastled_light(config)

    rgb_order = None
    if CONF_RGB_ORDER in config:
        rgb_order = cg.RawExpression(config[CONF_RGB_ORDER])
    template_args = cg.TemplateArguments(cg.RawExpression(config[CONF_CHIPSET]),
                                         config[CONF_DATA_PIN], config[CONF_CLOCK_PIN], rgb_order)
    cg.add(var.add_leds(template_args, config[CONF_NUM_LEDS]))

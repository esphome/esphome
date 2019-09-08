import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import CONF_EXTERNAL_VCC, CONF_LAMBDA, CONF_MODEL, CONF_RESET_PIN, \
    CONF_BRIGHTNESS
from esphome.core import coroutine

ssd1306_base_ns = cg.esphome_ns.namespace('ssd1306_base')
SSD1306 = ssd1306_base_ns.class_('SSD1306', cg.PollingComponent, display.DisplayBuffer)
SSD1306Model = ssd1306_base_ns.enum('SSD1306Model')

MODELS = {
    'SSD1306_128X32': SSD1306Model.SSD1306_MODEL_128_32,
    'SSD1306_128X64': SSD1306Model.SSD1306_MODEL_128_64,
    'SSD1306_96X16': SSD1306Model.SSD1306_MODEL_96_16,
    'SSD1306_64X48': SSD1306Model.SSD1306_MODEL_64_48,
    'SH1106_128X32': SSD1306Model.SH1106_MODEL_128_32,
    'SH1106_128X64': SSD1306Model.SH1106_MODEL_128_64,
    'SH1106_96X16': SSD1306Model.SH1106_MODEL_96_16,
    'SH1106_64X48': SSD1306Model.SH1106_MODEL_64_48,
}

SSD1306_MODEL = cv.enum(MODELS, upper=True, space="_")

SSD1306_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend({
    cv.Required(CONF_MODEL): SSD1306_MODEL,
    cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.percentage,
    cv.Optional(CONF_EXTERNAL_VCC): cv.boolean,
}).extend(cv.polling_component_schema('1s'))


@coroutine
def setup_ssd1036(var, config):
    yield cg.register_component(var, config)
    yield display.register_display(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))
    if CONF_RESET_PIN in config:
        reset = yield cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_BRIGHTNESS in config:
        cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))
    if CONF_EXTERNAL_VCC in config:
        cg.add(var.set_external_vcc(config[CONF_EXTERNAL_VCC]))
    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, 'it')], return_type=cg.void)
        cg.add(var.set_writer(lambda_))

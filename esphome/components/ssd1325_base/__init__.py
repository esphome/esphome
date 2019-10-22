import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import CONF_EXTERNAL_VCC, CONF_LAMBDA, CONF_MODEL, CONF_RESET_PIN
from esphome.core import coroutine

ssd1325_base_ns = cg.esphome_ns.namespace('ssd1325_base')
SSD1325 = ssd1325_base_ns.class_('SSD1325', cg.PollingComponent, display.DisplayBuffer)
SSD1325Model = ssd1325_base_ns.enum('SSD1325Model')

MODELS = {
    'SSD1325_128X32': SSD1325Model.SSD1325_MODEL_128_32,
    'SSD1325_128X64': SSD1325Model.SSD1325_MODEL_128_64,
    'SSD1325_96X16': SSD1325Model.SSD1325_MODEL_96_16,
    'SSD1325_64X48': SSD1325Model.SSD1325_MODEL_64_48,
}

SSD1325_MODEL = cv.enum(MODELS, upper=True, space="_")

SSD1325_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend({
    cv.Required(CONF_MODEL): SSD1325_MODEL,
    cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
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
    if CONF_EXTERNAL_VCC in config:
        cg.add(var.set_external_vcc(config[CONF_EXTERNAL_VCC]))
    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, 'it')], return_type=cg.void)
        cg.add(var.set_writer(lambda_))

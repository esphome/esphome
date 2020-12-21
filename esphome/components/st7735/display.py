import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi
from esphome.components import display
from esphome.core import coroutine
from esphome.const import CONF_DC_PIN, CONF_ID, CONF_LAMBDA, CONF_MODEL, CONF_RESET_PIN, CONF_PAGES
from . import st7735_ns

CODEOWNERS = ['@SenexCrenshaw']

DEPENDENCIES = ['spi']

CONF_DEVICEWIDTH = 'devicewidth'
CONF_DEVICEHEIGHT = 'deviceheight'
CONF_ROWSTART = 'rowstart'
CONF_COLSTART = 'colstart'
CONF_EIGHTBITCOLOR = 'eightbitcolor'
CONF_USEBGR = 'usebgr'

SPIST7735 = st7735_ns.class_('ST7735', cg.PollingComponent, display.DisplayBuffer, spi.SPIDevice)
ST7735Model = st7735_ns.enum('ST7735Model')

MODELS = {
    'INITR_GREENTAB': ST7735Model.ST7735_INITR_GREENTAB,
    'INITR_REDTAB': ST7735Model.ST7735_INITR_REDTAB,
    'INITR_BLACKTAB': ST7735Model.ST7735_INITR_BLACKTAB,
    'INITR_MINI160X80': ST7735Model.ST7735_INITR_MINI_160X80,
    'INITR_18BLACKTAB':  ST7735Model.ST7735_INITR_18BLACKTAB,
    'INITR_18REDTAB': ST7735Model.ST7735_INITR_18REDTAB
}
ST7735_MODEL = cv.enum(MODELS, upper=True, space="_")


ST7735_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend({
    cv.Required(CONF_MODEL): ST7735_MODEL,
    cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema
}).extend(cv.polling_component_schema('1s'))

CONFIG_SCHEMA = cv.All(ST7735_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(SPIST7735),
    cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_DEVICEWIDTH): cv.int_,
    cv.Required(CONF_DEVICEHEIGHT):  cv.int_,
    cv.Required(CONF_COLSTART):  cv.int_,
    cv.Required(CONF_ROWSTART):  cv.int_,
    cv.Optional(CONF_EIGHTBITCOLOR, default=False): cv.boolean,
    cv.Optional(CONF_USEBGR, default=False):  cv.boolean,
}).extend(cv.COMPONENT_SCHEMA).extend(spi.spi_device_schema()),
                       cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA))


@coroutine
def setup_st7735(var, config):
    yield cg.register_component(var, config)
    yield display.register_display(var, config)

    if CONF_RESET_PIN in config:
        reset = yield cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, 'it')], return_type=cg.void)
        cg.add(var.set_writer(lambda_))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_MODEL], config[CONF_DEVICEWIDTH],
                           config[CONF_DEVICEHEIGHT], config[CONF_COLSTART], config[CONF_ROWSTART],
                           config[CONF_EIGHTBITCOLOR], config[CONF_USEBGR])
    yield setup_st7735(var, config)
    yield spi.register_spi_device(var, config)

    dc = yield cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

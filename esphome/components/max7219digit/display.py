import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, spi
from esphome.const import CONF_ID, CONF_INTENSITY, CONF_LAMBDA, CONF_NUM_CHIPS

DEPENDENCIES = ['spi']

CONF_ROTATE_CHIP = 'rotate_chip'
CONF_SCROLL_SPEED = 'scroll_speed'
CONF_SCROLL_DWELL = 'scroll_dwell'
CONF_SCROLL_DELAY = 'scroll_delay'
bool CONF_SCROLL_ONOFF = 'scroll_enable'
CONF_SCROLL_MODE = 'scroll_mode'

max7219_ns = cg.esphome_ns.namespace('max7219digit')
MAX7219Component = max7219_ns.class_('MAX7219Component', cg.PollingComponent, spi.SPIDevice,
                                     display.DisplayBuffer)
MAX7219ComponentRef = MAX7219Component.operator('ref')

CONFIG_SCHEMA = display.BASIC_DISPLAY_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(MAX7219Component),
    cv.Optional(CONF_NUM_CHIPS, default=4): cv.int_range(min=1, max=255),
    cv.Optional(CONF_INTENSITY, default=15): cv.int_range(min=0, max=15),
    cv.Optional(CONF_ROTATE_CHIP, default=0): cv.int_range(min=0, max=4),
    cv.Optional(CONF_SCROLL_MODE, default=0): cv.int_range(min=0, max=1),
    cv.Optional(CONF_SCROLL_ONOFF, default=True): cv.boolean,
    cv.Optional(CONF_SCROLL_SPEED, default=250): cv.int_range(min=0),
    cv.Optional(CONF_SCROLL_DELAY, default=1000): cv.int_range(min=0),
    cv.Optional(CONF_SCROLL_DWELL, default=1000): cv.int_range(min=0),
}).extend(cv.polling_component_schema('500ms')).extend(spi.SPI_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)
    yield display.register_display(var, config)

    cg.add(var.set_num_chips(config[CONF_NUM_CHIPS]))
    cg.add(var.set_intensity(config[CONF_INTENSITY]))
    cg.add(var.set_chip_orientation(config[CONF_ROTATE_CHIP]))
    cg.add(var.set_scroll_speed(config[CONF_SCROLL_SPEED]))
    cg.add(var.set_scroll_dwell(config[CONF_SCROLL_DWELL]))
    cg.add(var.set_scroll_delay(config[CONF_SCROLL_DELAY]))
    cg.add(var.set_scroll(config[CONF_SCROLL_ONOFF]))
    cg.add(var.set_scroll_mode(config[CONF_SCROLL_MODE]))

    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(config[CONF_LAMBDA], [(MAX7219ComponentRef, 'it')],
                                          return_type=cg.void)
        cg.add(var.set_writer(lambda_))

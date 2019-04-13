from esphome.components import display, i2c
from esphome.components.display.lcd_gpio import LCDDisplay, LCDDisplayRef, \
    validate_lcd_dimensions
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ADDRESS, CONF_DIMENSIONS, CONF_ID, CONF_LAMBDA


DEPENDENCIES = ['i2c']

PCF8574LCDDisplay = display.display_ns.class_('PCF8574LCDDisplay', LCDDisplay, i2c.I2CDevice)

PLATFORM_SCHEMA = display.BASIC_DISPLAY_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(PCF8574LCDDisplay),
    cv.Required(CONF_DIMENSIONS): validate_lcd_dimensions,
    cv.Optional(CONF_ADDRESS): cv.i2c_address,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    rhs = App.make_pcf8574_lcd_display(config[CONF_DIMENSIONS][0], config[CONF_DIMENSIONS][1])
    lcd = Pvariable(config[CONF_ID], rhs)

    if CONF_ADDRESS in config:
        cg.add(lcd.set_address(config[CONF_ADDRESS]))

    if CONF_LAMBDA in config:
        lambda_ = yield process_lambda(config[CONF_LAMBDA], [(LCDDisplayRef, 'it')],
                                       return_type=void)
        cg.add(lcd.set_writer(lambda_))

    display.setup_display(lcd, config)
    register_component(lcd, config)


BUILD_FLAGS = ['-DUSE_LCD_DISPLAY', '-DUSE_LCD_DISPLAY_PCF8574']

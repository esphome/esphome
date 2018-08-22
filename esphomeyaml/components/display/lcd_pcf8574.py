import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import display
from esphomeyaml.components.display.lcd_gpio import LCDDisplayRef, validate_lcd_dimensions
from esphomeyaml.const import CONF_ADDRESS, CONF_DIMENSIONS, CONF_ID, CONF_LAMBDA
from esphomeyaml.helpers import App, Pvariable, add, process_lambda

DEPENDENCIES = ['i2c']

PCF8574LCDDisplay = display.display_ns.PCF8574LCDDisplay

PLATFORM_SCHEMA = display.BASIC_DISPLAY_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(PCF8574LCDDisplay),
    vol.Required(CONF_DIMENSIONS): validate_lcd_dimensions,
    vol.Optional(CONF_ADDRESS): cv.i2c_address,
})


def to_code(config):
    rhs = App.make_pcf8574_lcd_display(config[CONF_DIMENSIONS][0], config[CONF_DIMENSIONS][1])
    lcd = Pvariable(config[CONF_ID], rhs)

    if CONF_ADDRESS in config:
        add(lcd.set_address(config[CONF_ADDRESS]))

    if CONF_LAMBDA in config:
        for lambda_ in process_lambda(config[CONF_LAMBDA], [(LCDDisplayRef, 'it')]):
            yield
        add(lcd.set_writer(lambda_))

    display.setup_display(lcd, config)


BUILD_FLAGS = ['-DUSE_LCD_DISPLAY', '-DUSE_LCD_DISPLAY_PCF8574']

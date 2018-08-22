import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import display
from esphomeyaml.components.display import ssd1306_spi
from esphomeyaml.const import CONF_ADDRESS, CONF_EXTERNAL_VCC, CONF_ID, CONF_MODEL, CONF_RESET_PIN
from esphomeyaml.helpers import App, Pvariable, add, gpio_output_pin_expression

DEPENDENCIES = ['i2c']

I2CSSD1306 = display.display_ns.I2CSSD1306

PLATFORM_SCHEMA = display.FULL_DISPLAY_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(I2CSSD1306),
    vol.Required(CONF_MODEL): ssd1306_spi.SSD1306_MODEL,
    vol.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_EXTERNAL_VCC): cv.boolean,
    vol.Optional(CONF_ADDRESS): cv.i2c_address,
})


def to_code(config):
    ssd = Pvariable(config[CONF_ID], App.make_i2c_ssd1306())
    add(ssd.set_model(ssd1306_spi.MODELS[config[CONF_MODEL]]))

    if CONF_RESET_PIN in config:
        for reset in gpio_output_pin_expression(config[CONF_RESET_PIN]):
            yield
        add(ssd.set_reset_pin(reset))
    if CONF_EXTERNAL_VCC in config:
        add(ssd.set_external_vcc(config[CONF_EXTERNAL_VCC]))
    if CONF_ADDRESS in config:
        add(ssd.set_address(config[CONF_ADDRESS]))

    display.setup_display(ssd, config)


BUILD_FLAGS = '-DUSE_SSD1306'

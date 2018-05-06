import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.const import CONF_FREQUENCY, CONF_SCL, CONF_SDA, CONF_SCAN, CONF_ID
from esphomeyaml.helpers import App, add, Pvariable

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID('i2c'): cv.register_variable_id,
    vol.Required(CONF_SDA, default='SDA'): pins.input_output_pin,
    vol.Required(CONF_SCL, default='SCL'): pins.input_output_pin,
    vol.Optional(CONF_FREQUENCY): vol.All(cv.only_on_esp32, cv.positive_int),
    vol.Optional(CONF_SCAN): cv.boolean,
})


def to_code(config):
    rhs = App.init_i2c(config[CONF_SDA], config[CONF_SCL], config.get(CONF_SCAN))
    i2c = Pvariable('I2CComponent', config[CONF_ID], rhs)
    if CONF_FREQUENCY in config:
        add(i2c.set_frequency(config[CONF_FREQUENCY]))


BUILD_FLAGS = '-DUSE_I2C'

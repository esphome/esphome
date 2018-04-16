import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.const import CONF_FREQUENCY, CONF_SCL, CONF_SDA
from esphomeyaml.helpers import App, add

CONFIG_SCHEMA = vol.Schema({
    vol.Required(CONF_SDA, default='SDA'): pins.input_output_pin,
    vol.Required(CONF_SCL, default='SCL'): pins.input_output_pin,
    vol.Optional(CONF_FREQUENCY): vol.All(cv.only_on_esp32, cv.positive_int),
})


def to_code(config):
    add(App.init_i2c(config[CONF_SDA], config[CONF_SCL], config.get(CONF_FREQUENCY)))


def build_flags(config):
    return '-DUSE_I2C'

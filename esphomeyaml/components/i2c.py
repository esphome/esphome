import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.const import CONF_FREQUENCY, CONF_SCL, CONF_SDA, CONF_SCAN, CONF_ID, \
    CONF_RECEIVE_TIMEOUT
from esphomeyaml.helpers import App, add, Pvariable, esphomelib_ns

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID('i2c'): cv.register_variable_id,
    vol.Required(CONF_SDA, default='SDA'): pins.input_output_pin,
    vol.Required(CONF_SCL, default='SCL'): pins.input_output_pin,
    vol.Optional(CONF_FREQUENCY): cv.positive_int,
    vol.Optional(CONF_RECEIVE_TIMEOUT): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_SCAN): cv.boolean,
})

I2CComponent = esphomelib_ns.I2CComponent


def to_code(config):
    rhs = App.init_i2c(config[CONF_SDA], config[CONF_SCL], config.get(CONF_SCAN))
    i2c = Pvariable(I2CComponent, config[CONF_ID], rhs)
    if CONF_FREQUENCY in config:
        add(i2c.set_frequency(config[CONF_FREQUENCY]))
    if CONF_RECEIVE_TIMEOUT in config:
        add(i2c.set_receive_timeout(config[CONF_RECEIVE_TIMEOUT]))


BUILD_FLAGS = '-DUSE_I2C'

LIB_DEPS = 'Wire'

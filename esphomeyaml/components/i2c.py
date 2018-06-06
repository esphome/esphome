import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.const import CONF_FREQUENCY, CONF_SCL, CONF_SDA, CONF_SCAN, CONF_ID, \
    CONF_RECEIVE_TIMEOUT
from esphomeyaml.helpers import App, add, Pvariable, esphomelib_ns

I2CComponent = esphomelib_ns.I2CComponent

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(I2CComponent),
    vol.Required(CONF_SDA, default='SDA'): pins.input_output_pin,
    vol.Required(CONF_SCL, default='SCL'): pins.input_output_pin,
    vol.Optional(CONF_FREQUENCY): vol.All(cv.frequency, vol.Range(min=0, min_included=False)),
    vol.Optional(CONF_RECEIVE_TIMEOUT): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_SCAN): cv.boolean,
})


def to_code(config):
    rhs = App.init_i2c(config[CONF_SDA], config[CONF_SCL], config.get(CONF_SCAN))
    i2c = Pvariable(config[CONF_ID], rhs)
    if CONF_FREQUENCY in config:
        add(i2c.set_frequency(config[CONF_FREQUENCY]))
    if CONF_RECEIVE_TIMEOUT in config:
        add(i2c.set_receive_timeout(config[CONF_RECEIVE_TIMEOUT]))


BUILD_FLAGS = '-DUSE_I2C'

LIB_DEPS = 'Wire'

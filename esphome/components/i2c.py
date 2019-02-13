import voluptuous as vol

from esphome import pins
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY, CONF_ID, CONF_RECEIVE_TIMEOUT, CONF_SCAN, CONF_SCL, \
    CONF_SDA
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component, esphome_ns

I2CComponent = esphome_ns.class_('I2CComponent', Component)
I2CDevice = pins.I2CDevice

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(I2CComponent),
    vol.Optional(CONF_SDA, default='SDA'): pins.input_pullup_pin,
    vol.Optional(CONF_SCL, default='SCL'): pins.input_pullup_pin,
    vol.Optional(CONF_FREQUENCY): vol.All(cv.frequency, vol.Range(min=0, min_included=False)),
    vol.Optional(CONF_SCAN): cv.boolean,

    vol.Optional(CONF_RECEIVE_TIMEOUT): cv.invalid("The receive_timeout option has been removed "
                                                   "because timeouts are already handled by the "
                                                   "low-level i2c interface.")
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.init_i2c(config[CONF_SDA], config[CONF_SCL], config.get(CONF_SCAN))
    i2c = Pvariable(config[CONF_ID], rhs)
    if CONF_FREQUENCY in config:
        add(i2c.set_frequency(config[CONF_FREQUENCY]))

    setup_component(i2c, config)


BUILD_FLAGS = '-DUSE_I2C'

LIB_DEPS = 'Wire'

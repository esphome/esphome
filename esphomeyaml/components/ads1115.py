import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADDRESS, CONF_ID, CONF_RATE
from esphomeyaml.helpers import App, Pvariable

DEPENDENCIES = ['i2c']

ADS1115Component = sensor.sensor_ns.ADS1115Component

RATE_REMOVE_MESSAGE = """The rate option has been removed in 1.5.0 and is no longer required."""

ADS1115_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(ADS1115Component),
    vol.Required(CONF_ADDRESS): cv.i2c_address,

    vol.Optional(CONF_RATE): cv.invalid(RATE_REMOVE_MESSAGE)
})

CONFIG_SCHEMA = vol.All(cv.ensure_list, [ADS1115_SCHEMA])


def to_code(config):
    for conf in config:
        rhs = App.make_ads1115_component(conf[CONF_ADDRESS])
        Pvariable(conf[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_ADS1115_SENSOR'

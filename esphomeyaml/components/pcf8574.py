import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ADDRESS, CONF_ID, CONF_PCF8575
from esphomeyaml.helpers import App, Pvariable, esphomelib_ns

DEPENDENCIES = ['i2c']

io_ns = esphomelib_ns.namespace('io')
PCF8574Component = io_ns.PCF8574Component

PCF8574_SCHEMA = vol.Schema({
    vol.Required(CONF_ID): cv.declare_variable_id(PCF8574Component),
    vol.Optional(CONF_ADDRESS, default=0x21): cv.i2c_address,
    vol.Optional(CONF_PCF8575, default=False): cv.boolean,
})

CONFIG_SCHEMA = vol.All(cv.ensure_list, [PCF8574_SCHEMA])


def to_code(config):
    for conf in config:
        rhs = App.make_pcf8574_component(conf[CONF_ADDRESS], conf[CONF_PCF8575])
        Pvariable(conf[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_PCF8574'

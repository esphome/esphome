import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ADDRESS, CONF_ID, CONF_PCF8575
from esphomeyaml.helpers import App, Pvariable, esphomelib_ns

DEPENDENCIES = ['i2c']

PCF8574_SCHEMA = vol.Schema({
    vol.Required(CONF_ID): cv.register_variable_id,
    vol.Optional(CONF_ADDRESS, default=0x21): cv.i2c_address,
    vol.Optional(CONF_PCF8575, default=False): cv.boolean,
})

CONFIG_SCHEMA = vol.All(cv.ensure_list, [PCF8574_SCHEMA])

io_ns = esphomelib_ns.namespace('io')
PCF8574Component = io_ns.PCF8574Component


def to_code(config):
    for conf in config:
        rhs = App.make_pcf8574_component(conf[CONF_ADDRESS], conf[CONF_PCF8575])
        Pvariable(PCF8574Component, conf[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_PCF8574'

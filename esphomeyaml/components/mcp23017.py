import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ADDRESS, CONF_ID
from esphomeyaml.helpers import App, Pvariable, esphomelib_ns

DEPENDENCIES = ['i2c']

io_ns = esphomelib_ns.namespace('io')
MCP23017Component = io_ns.MCP23017Component

MCP23017_SCHEMA = vol.Schema({
    vol.Required(CONF_ID): cv.declare_variable_id(MCP23017Component),
    vol.Optional(CONF_ADDRESS, default=0x20): cv.i2c_address,
})

CONFIG_SCHEMA = vol.All(cv.ensure_list, [MCP23017_SCHEMA])


def to_code(config):
    for conf in config:
        rhs = App.make_mcp23017_component(conf[CONF_ADDRESS])
        Pvariable(conf[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_MCP23017'

import voluptuous as vol

from esphome import pins
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, GPIOInputPin, GPIOOutputPin, io_ns, esphome_ns

DEPENDENCIES = ['i2c']
MULTI_CONF = True

MCP23017GPIOMode = esphome_ns.enum('MCP23017GPIOMode')
MCP23017_GPIO_MODES = {
    'INPUT': MCP23017GPIOMode.MCP23017_INPUT,
    'INPUT_PULLUP': MCP23017GPIOMode.MCP23017_INPUT_PULLUP,
    'OUTPUT': MCP23017GPIOMode.MCP23017_OUTPUT,
}

MCP23017GPIOInputPin = io_ns.class_('MCP23017GPIOInputPin', GPIOInputPin)
MCP23017GPIOOutputPin = io_ns.class_('MCP23017GPIOOutputPin', GPIOOutputPin)

CONFIG_SCHEMA = cv.Schema({
    vol.Required(CONF_ID): cv.declare_variable_id(pins.MCP23017),
    vol.Optional(CONF_ADDRESS, default=0x20): cv.i2c_address,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_mcp23017_component(config[CONF_ADDRESS])
    var = Pvariable(config[CONF_ID], rhs)
    setup_component(var, config)


BUILD_FLAGS = '-DUSE_MCP23017'

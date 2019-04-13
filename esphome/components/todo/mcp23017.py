from esphome import pins
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ADDRESS, CONF_ID


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
    cv.Required(CONF_ID): cv.declare_variable_id(pins.MCP23017),
    cv.Optional(CONF_ADDRESS, default=0x20): cv.i2c_address,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    rhs = App.make_mcp23017_component(config[CONF_ADDRESS])
    var = Pvariable(config[CONF_ID], rhs)
    register_component(var, config)


BUILD_FLAGS = '-DUSE_MCP23017'

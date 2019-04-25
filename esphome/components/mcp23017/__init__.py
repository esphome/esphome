import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_NUMBER, CONF_MODE, CONF_INVERTED

DEPENDENCIES = ['i2c']
MULTI_CONF = True

mcp23017_ns = cg.esphome_ns.namespace('mcp23017')
MCP23017GPIOMode = mcp23017_ns.enum('MCP23017GPIOMode')
MCP23017_GPIO_MODES = {
    'INPUT': MCP23017GPIOMode.MCP23017_INPUT,
    'INPUT_PULLUP': MCP23017GPIOMode.MCP23017_INPUT_PULLUP,
    'OUTPUT': MCP23017GPIOMode.MCP23017_OUTPUT,
}

MCP23017 = mcp23017_ns.class_('MCP23017', cg.Component, i2c.I2CDevice)
MCP23017GPIOPin = mcp23017_ns.class_('MCP23017GPIOPin', cg.GPIOPin)

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(MCP23017),
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x20))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)


CONF_MCP23017 = 'mcp23017'
MCP23017_OUTPUT_PIN_SCHEMA = cv.Schema({
    cv.Required(CONF_MCP23017): cv.use_id(MCP23017),
    cv.Required(CONF_NUMBER): cv.int_,
    cv.Optional(CONF_MODE, default="OUTPUT"): cv.enum(MCP23017_GPIO_MODES, upper=True),
    cv.Optional(CONF_INVERTED, default=False): cv.boolean,
})
MCP23017_INPUT_PIN_SCHEMA = cv.Schema({
    cv.Required(CONF_MCP23017): cv.use_id(MCP23017),
    cv.Required(CONF_NUMBER): cv.int_,
    cv.Optional(CONF_MODE, default="INPUT"): cv.enum(MCP23017_GPIO_MODES, upper=True),
    cv.Optional(CONF_INVERTED, default=False): cv.boolean,
})


@pins.PIN_SCHEMA_REGISTRY.register(CONF_MCP23017,
                                   (MCP23017_OUTPUT_PIN_SCHEMA, MCP23017_INPUT_PIN_SCHEMA))
def mcp23017_pin_to_code(config):
    parent = yield cg.get_variable(config[CONF_MCP23017])
    yield MCP23017GPIOPin.new(parent, config[CONF_NUMBER], config[CONF_MODE], config[CONF_INVERTED])

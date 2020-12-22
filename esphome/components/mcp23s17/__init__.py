import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi
from esphome.const import CONF_ID, CONF_NUMBER, CONF_MODE, CONF_INVERTED

CODEOWNERS = ['@SenexCrenshaw']

DEPENDENCIES = ['spi']
MULTI_CONF = True

CONF_DEVICEADDRESS = "deviceaddress"

mcp23S17_ns = cg.esphome_ns.namespace('mcp23s17')
mcp23S17GPIOMode = mcp23S17_ns.enum('MCP23S17GPIOMode')
mcp23S17_GPIO_MODES = {
    'INPUT': mcp23S17GPIOMode.MCP23S17_INPUT,
    'INPUT_PULLUP': mcp23S17GPIOMode.MCP23S17_INPUT_PULLUP,
    'OUTPUT': mcp23S17GPIOMode.MCP23S17_OUTPUT,
}

mcp23S17 = mcp23S17_ns.class_('MCP23S17', cg.Component, spi.SPIDevice)
mcp23S17GPIOPin = mcp23S17_ns.class_('MCP23S17GPIOPin', cg.GPIOPin)

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(mcp23S17),
    cv.Optional(CONF_DEVICEADDRESS, default=0): cv.uint8_t,
}).extend(cv.COMPONENT_SCHEMA).extend(spi.spi_device_schema())


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_device_address(config[CONF_DEVICEADDRESS]))
    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)


CONF_MCP23S17 = 'mcp23s17'

mcp23S17_OUTPUT_PIN_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_MCP23S17): cv.use_id(mcp23S17),
    cv.Required(CONF_NUMBER): cv.int_,
    cv.Optional(CONF_MODE, default="OUTPUT"): cv.enum(mcp23S17_GPIO_MODES, upper=True),
    cv.Optional(CONF_INVERTED, default=False): cv.boolean,
})
mcp23S17_INPUT_PIN_SCHEMA = cv.Schema({
    cv.Required(CONF_MCP23S17): cv.use_id(mcp23S17),
    cv.Required(CONF_NUMBER): cv.int_range(0, 15),
    cv.Optional(CONF_MODE, default="INPUT"): cv.enum(mcp23S17_GPIO_MODES, upper=True),
    cv.Optional(CONF_INVERTED, default=False): cv.boolean,
})


@pins.PIN_SCHEMA_REGISTRY.register(CONF_MCP23S17,
                                   (mcp23S17_OUTPUT_PIN_SCHEMA, mcp23S17_INPUT_PIN_SCHEMA))
def mcp23S17_pin_to_code(config):
    parent = yield cg.get_variable(config[CONF_MCP23S17])
    yield mcp23S17GPIOPin.new(parent, config[CONF_NUMBER], config[CONF_MODE], config[CONF_INVERTED])

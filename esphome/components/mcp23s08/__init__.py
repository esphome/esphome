import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi
from esphome.const import CONF_ID, CONF_NUMBER, CONF_MODE, CONF_INVERTED, \
    CONF_ADDRESS

DEPENDENCIES = ['spi']
MULTI_CONF = True

mcp23s08_ns = cg.esphome_ns.namespace('mcp23s08')
MCP23S08GPIOMode = mcp23s08_ns.enum('MCP23S08GPIOMode')
MCP23S08_GPIO_MODES = {
    'INPUT': MCP23S08GPIOMode.MCP23S08_INPUT,
    'INPUT_PULLUP': MCP23S08GPIOMode.MCP23S08_INPUT_PULLUP,
    'OUTPUT': MCP23S08GPIOMode.MCP23S08_OUTPUT,
}

MCP23S08 = mcp23s08_ns.class_('MCP23S08', cg.Component, spi.SPIDevice)
MCP23S08GPIOPin = mcp23s08_ns.class_('MCP23S08GPIOPin', cg.GPIOPin)

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(MCP23S08),
    cv.Optional(CONF_ADDRESS, default=0x40): cv.hex_uint8_t,
}).extend(cv.COMPONENT_SCHEMA).extend(
    spi.spi_device_schema(cs_pin_required=True))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_address(config[CONF_ADDRESS]))
    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)


CONF_MCP23S08 = 'mcp23s08'
MCP23S08_OUTPUT_PIN_SCHEMA = cv.Schema({
    cv.Required(CONF_MCP23S08): cv.use_id(MCP23S08),
    cv.Required(CONF_NUMBER): cv.int_,
    cv.Optional(CONF_MODE, default="OUTPUT"): cv.enum(MCP23S08_GPIO_MODES,
                                                      upper=True),
    cv.Optional(CONF_INVERTED, default=False): cv.boolean,
})
MCP23S08_INPUT_PIN_SCHEMA = cv.Schema({
    cv.Required(CONF_MCP23S08): cv.use_id(MCP23S08),
    cv.Required(CONF_NUMBER): cv.int_,
    cv.Optional(CONF_MODE, default="INPUT"): cv.enum(MCP23S08_GPIO_MODES,
                                                     upper=True),
    cv.Optional(CONF_INVERTED, default=False): cv.boolean,
})


@pins.PIN_SCHEMA_REGISTRY.register(CONF_MCP23S08,
                                   (MCP23S08_OUTPUT_PIN_SCHEMA,
                                    MCP23S08_INPUT_PIN_SCHEMA))
def mcp23008_pin_to_code(config):
    parent = yield cg.get_variable(config[CONF_MCP23S08])
    yield MCP23S08GPIOPin.new(parent, config[CONF_NUMBER],
                              config[CONF_MODE], config[CONF_INVERTED])

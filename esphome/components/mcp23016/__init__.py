import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_NUMBER, CONF_MODE, CONF_INVERTED

DEPENDENCIES = ["i2c"]
MULTI_CONF = True

mcp23016_ns = cg.esphome_ns.namespace("mcp23016")
MCP23016GPIOMode = mcp23016_ns.enum("MCP23016GPIOMode")
MCP23016_GPIO_MODES = {
    "INPUT": MCP23016GPIOMode.MCP23016_INPUT,
    "OUTPUT": MCP23016GPIOMode.MCP23016_OUTPUT,
}

MCP23016 = mcp23016_ns.class_("MCP23016", cg.Component, i2c.I2CDevice)
MCP23016GPIOPin = mcp23016_ns.class_("MCP23016GPIOPin", cg.GPIOPin)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(MCP23016),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x20))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)


CONF_MCP23016 = "mcp23016"
MCP23016_OUTPUT_PIN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_MCP23016): cv.use_id(MCP23016),
        cv.Required(CONF_NUMBER): cv.int_,
        cv.Optional(CONF_MODE, default="OUTPUT"): cv.enum(
            MCP23016_GPIO_MODES, upper=True
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)
MCP23016_INPUT_PIN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_MCP23016): cv.use_id(MCP23016),
        cv.Required(CONF_NUMBER): cv.int_,
        cv.Optional(CONF_MODE, default="INPUT"): cv.enum(
            MCP23016_GPIO_MODES, upper=True
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(
    CONF_MCP23016, (MCP23016_OUTPUT_PIN_SCHEMA, MCP23016_INPUT_PIN_SCHEMA)
)
async def mcp23016_pin_to_code(config):
    parent = await cg.get_variable(config[CONF_MCP23016])
    return MCP23016GPIOPin.new(
        parent, config[CONF_NUMBER], config[CONF_MODE], config[CONF_INVERTED]
    )

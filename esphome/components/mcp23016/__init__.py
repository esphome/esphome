from esphome import pins
import esphome.codegen as cg
from esphome.components import gpio_expander, i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INPUT, CONF_INTERRUPT_PIN, CONF_OUTPUT
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["gpio_expander"]
DEPENDENCIES = ["i2c"]
DOMAIN = "mcp23016"
MULTI_CONF = True

mcp23016_ns = cg.esphome_ns.namespace("mcp23016")

MCP23016 = mcp23016_ns.class_("MCP23016", cg.Component, i2c.I2CDevice)
MCP23016GPIOPin = mcp23016_ns.class_("MCP23016GPIOPin", cg.GPIOPin)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(MCP23016),
        }
    )
    .extend(gpio_expander.INTERRUPT_EXPANDER_SCHEMA)
    .extend(i2c.i2c_device_schema(0x20))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    await cg.add_pin(var.set_interrupt_pin, config.get(CONF_INTERRUPT_PIN))


CONF_MCP23016 = "mcp23016"
MCP23016_PIN_SCHEMA = pins.gpio_base_schema(
    MCP23016GPIOPin,
    cv.int_range(min=0, max=15),
    modes=[CONF_INPUT, CONF_OUTPUT],
    mode_validator=pins.gpio_validate_input_xor_output,
    invertible=True,
).extend(
    {
        cv.Required(CONF_MCP23016): pins.use_id_or_address(MCP23016),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_MCP23016, MCP23016_PIN_SCHEMA)
async def mcp23016_pin_to_code(config: ConfigType) -> MockObj:
    return await pins.new_parented_pin(config, CONF_MCP23016)

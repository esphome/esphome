import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_NUMBER,
    CONF_MODE,
    CONF_INVERTED,
    CONF_INTERRUPT,
    CONF_OPEN_DRAIN_INTERRUPT,
    CONF_OUTPUT,
    CONF_PULLUP,
)
from esphome.core import coroutine

CODEOWNERS = ["@jesserockz"]

mcp23xxx_base_ns = cg.esphome_ns.namespace("mcp23xxx_base")
MCP23XXXBase = mcp23xxx_base_ns.class_("MCP23XXXBase", cg.Component)
MCP23XXXGPIOPin = mcp23xxx_base_ns.class_("MCP23XXXGPIOPin", cg.GPIOPin)
MCP23XXXInterruptMode = mcp23xxx_base_ns.enum("MCP23XXXInterruptMode")

MCP23XXX_INTERRUPT_MODES = {
    "NO_INTERRUPT": MCP23XXXInterruptMode.MCP23XXX_NO_INTERRUPT,
    "CHANGE": MCP23XXXInterruptMode.MCP23XXX_CHANGE,
    "RISING": MCP23XXXInterruptMode.MCP23XXX_RISING,
    "FALLING": MCP23XXXInterruptMode.MCP23XXX_FALLING,
}

MCP23XXX_CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_OPEN_DRAIN_INTERRUPT, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


@coroutine
async def register_mcp23xxx(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_open_drain_ints(config[CONF_OPEN_DRAIN_INTERRUPT]))
    return var


def validate_mode(value):
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_PULLUP] and not value[CONF_INPUT]:
        raise cv.Invalid("Pullup only available with input")
    return value


CONF_MCP23XXX = "mcp23xxx"
MCP23XXX_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(MCP23XXXGPIOPin),
        cv.Required(CONF_MCP23XXX): cv.use_id(MCP23XXXBase),
        cv.Required(CONF_NUMBER): cv.int_range(min=0, max=15),
        cv.Optional(CONF_MODE, default={}): cv.All(
            {
                cv.Optional(CONF_INPUT, default=False): cv.boolean,
                cv.Optional(CONF_PULLUP, default=False): cv.boolean,
                cv.Optional(CONF_OUTPUT, default=False): cv.boolean,
            },
            validate_mode,
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
        cv.Optional(CONF_INTERRUPT, default="NO_INTERRUPT"): cv.enum(
            MCP23XXX_INTERRUPT_MODES, upper=True
        ),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_MCP23XXX, MCP23XXX_PIN_SCHEMA)
async def mcp23xxx_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_MCP23XXX])

    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    cg.add(var.set_interrupt_mode(config[CONF_INTERRUPT]))
    return var

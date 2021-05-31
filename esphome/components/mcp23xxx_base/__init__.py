import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_ID,
    CONF_NUMBER,
    CONF_MODE,
    CONF_INVERTED,
    CONF_INTERRUPT,
    CONF_OPEN_DRAIN_INTERRUPT,
)
from esphome.core import coroutine

CODEOWNERS = ["@jesserockz"]

mcp23xxx_base_ns = cg.esphome_ns.namespace("mcp23xxx_base")
MCP23XXXBase = mcp23xxx_base_ns.class_("MCP23XXXBase", cg.Component)
MCP23XXXGPIOPin = mcp23xxx_base_ns.class_("MCP23XXXGPIOPin", cg.GPIOPin)
MCP23XXXGPIOMode = mcp23xxx_base_ns.enum("MCP23XXXGPIOMode")
MCP23XXXInterruptMode = mcp23xxx_base_ns.enum("MCP23XXXInterruptMode")

MCP23XXX_INTERRUPT_MODES = {
    "NO_INTERRUPT": MCP23XXXInterruptMode.MCP23XXX_NO_INTERRUPT,
    "CHANGE": MCP23XXXInterruptMode.MCP23XXX_CHANGE,
    "RISING": MCP23XXXInterruptMode.MCP23XXX_RISING,
    "FALLING": MCP23XXXInterruptMode.MCP23XXX_FALLING,
}

MCP23XXX_GPIO_MODES = {
    "INPUT": MCP23XXXGPIOMode.MCP23XXX_INPUT,
    "INPUT_PULLUP": MCP23XXXGPIOMode.MCP23XXX_INPUT_PULLUP,
    "OUTPUT": MCP23XXXGPIOMode.MCP23XXX_OUTPUT,
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


CONF_MCP23XXX = "mcp23xxx"
MCP23XXX_OUTPUT_PIN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_MCP23XXX): cv.use_id(MCP23XXXBase),
        cv.Required(CONF_NUMBER): cv.int_,
        cv.Optional(CONF_MODE, default="OUTPUT"): cv.enum(
            MCP23XXX_GPIO_MODES, upper=True
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
        cv.Optional(CONF_INTERRUPT, default="NO_INTERRUPT"): cv.enum(
            MCP23XXX_INTERRUPT_MODES, upper=True
        ),
    }
)
MCP23XXX_INPUT_PIN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_MCP23XXX): cv.use_id(MCP23XXXBase),
        cv.Required(CONF_NUMBER): cv.int_,
        cv.Optional(CONF_MODE, default="INPUT"): cv.enum(
            MCP23XXX_GPIO_MODES, upper=True
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
        cv.Optional(CONF_INTERRUPT, default="NO_INTERRUPT"): cv.enum(
            MCP23XXX_INTERRUPT_MODES, upper=True
        ),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(
    CONF_MCP23XXX, (MCP23XXX_OUTPUT_PIN_SCHEMA, MCP23XXX_INPUT_PIN_SCHEMA)
)
async def mcp23xxx_pin_to_code(config):
    parent = await cg.get_variable(config[CONF_MCP23XXX])
    return MCP23XXXGPIOPin.new(
        parent,
        config[CONF_NUMBER],
        config[CONF_MODE],
        config[CONF_INVERTED],
        config[CONF_INTERRUPT],
    )


# BEGIN Removed pin schemas below to show error in configuration
# TODO remove in 1.19.0

for id in ["mcp23008", "mcp23s08", "mcp23017", "mcp23s17"]:
    PIN_SCHEMA = cv.Schema(
        {
            cv.Required(id): cv.invalid(
                f"'{id}:' has been removed from the pin schema in 1.17.0, please use 'mcp23xxx:'"
            ),
            cv.Required(CONF_NUMBER): cv.int_,
            cv.Optional(CONF_MODE, default="INPUT"): cv.enum(
                MCP23XXX_GPIO_MODES, upper=True
            ),
            cv.Optional(CONF_INVERTED, default=False): cv.boolean,
            cv.Optional(CONF_INTERRUPT, default="NO_INTERRUPT"): cv.enum(
                MCP23XXX_INTERRUPT_MODES, upper=True
            ),
        }
    )

    @pins.PIN_SCHEMA_REGISTRY.register(id, (PIN_SCHEMA, PIN_SCHEMA))
    def pin_to_code(config):
        pass


# END Removed pin schemas

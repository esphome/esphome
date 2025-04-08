import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_INITIAL_VALUE
from .. import Mcp4461Component, CONF_MCP4461_ID, mcp4461_ns

DEPENDENCIES = ["mcp4461"]

Mcp4461Wiper = mcp4461_ns.class_(
    "Mcp4461Wiper", output.FloatOutput, cg.Parented.template(Mcp4461Component)
)

Mcp4461WiperIdx = mcp4461_ns.enum("Mcp4461WiperIdx", is_class=True)
CHANNEL_OPTIONS = {
    "A": Mcp4461WiperIdx.MCP4461_WIPER_0,
    "B": Mcp4461WiperIdx.MCP4461_WIPER_1,
    "C": Mcp4461WiperIdx.MCP4461_WIPER_2,
    "D": Mcp4461WiperIdx.MCP4461_WIPER_3,
    "E": Mcp4461WiperIdx.MCP4461_WIPER_4,
    "F": Mcp4461WiperIdx.MCP4461_WIPER_5,
    "G": Mcp4461WiperIdx.MCP4461_WIPER_6,
    "H": Mcp4461WiperIdx.MCP4461_WIPER_7,
}

CONF_TERMINAL_A = "terminal_a"
CONF_TERMINAL_B = "terminal_b"
CONF_TERMINAL_W = "terminal_w"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(Mcp4461Wiper),
        cv.GenerateID(CONF_MCP4461_ID): cv.use_id(Mcp4461Component),
        cv.Required(CONF_CHANNEL): cv.enum(CHANNEL_OPTIONS, upper=True),
        cv.Optional(CONF_TERMINAL_A, default=True): cv.boolean,
        cv.Optional(CONF_TERMINAL_B, default=True): cv.boolean,
        cv.Optional(CONF_TERMINAL_W, default=True): cv.boolean,
        cv.Optional(CONF_INITIAL_VALUE): cv.float_range(min=0.0, max=1.0),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MCP4461_ID])
    var = cg.new_Pvariable(
        config[CONF_ID],
        parent,
        config[CONF_CHANNEL],
    )
    if not config[CONF_TERMINAL_A]:
        cg.add(parent.initialize_terminal_disabled(config[CONF_CHANNEL], "a"))
    if not config[CONF_TERMINAL_B]:
        cg.add(parent.initialize_terminal_disabled(config[CONF_CHANNEL], "b"))
    if not config[CONF_TERMINAL_W]:
        cg.add(parent.initialize_terminal_disabled(config[CONF_CHANNEL], "w"))
    if CONF_INITIAL_VALUE in config:
        cg.add(
            parent.set_initial_value(config[CONF_CHANNEL], config[CONF_INITIAL_VALUE])
        )
    await output.register_output(var, config)
    await cg.register_parented(var, config[CONF_MCP4461_ID])

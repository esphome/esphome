import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import output, i2c
from esphome.const import CONF_ID, CONF_CHANNEL
from . import MCP4728Output, mcp4728_ns

DEPENDENCIES = ["mcp4728"]

MCP4728Channel = mcp4728_ns.class_("MCP4728Channel", output.FloatOutput)
CONF_MCP4728_ID = "mcp4728_id"

MCP4727ChannelID = mcp4728_ns.enum("MCP4728ChannelType")
CHANNELS = {
    "A": MCP4727ChannelID.MCP4728_CHANNEL_A,
    "B": MCP4727ChannelID.MCP4728_CHANNEL_B,
    "C": MCP4727ChannelID.MCP4728_CHANNEL_C,
    "D": MCP4727ChannelID.MCP4728_CHANNEL_D
}

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(MCP4728Channel),
        cv.GenerateID(CONF_MCP4728_ID): cv.use_id(MCP4728Output),
        cv.Required(CONF_CHANNEL): cv.enum(CHANNELS, upper=True),
    }
)



async def to_code(config):
    paren = await cg.get_variable(config[CONF_MCP4728_ID])
    rhs = paren.create_channel(config[CONF_CHANNEL])
    var = cg.Pvariable(config[CONF_ID], rhs)
    await output.register_output(var, config)

import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_GAIN, CONF_ID

from .. import CONF_MCP4728_ID, MCP4728Component, mcp4728_ns

DEPENDENCIES = ["mcp4728"]

MCP4728Channel = mcp4728_ns.class_("MCP4728Channel", output.FloatOutput)
CONF_VREF = "vref"
CONF_POWER_DOWN = "power_down"

MCP4728Vref = mcp4728_ns.enum("MCP4728Vref")
VREF_OPTIONS = {
    "vdd": MCP4728Vref.MCP4728_VREF_VDD,
    "internal": MCP4728Vref.MCP4728_VREF_INTERNAL_2_8V,
}

MCP4728Gain = mcp4728_ns.enum("MCP4728Gain")
GAIN_OPTIONS = {"X1": MCP4728Gain.MCP4728_GAIN_X1, "X2": MCP4728Gain.MCP4728_GAIN_X2}

MCP4728PwrDown = mcp4728_ns.enum("MCP4728PwrDown")
PWRDOWN_OPTIONS = {
    "normal": MCP4728PwrDown.MCP4728_PD_NORMAL,
    "gnd_1k": MCP4728PwrDown.MCP4728_PD_GND_1KOHM,
    "gnd_100k": MCP4728PwrDown.MCP4728_PD_GND_100KOHM,
    "gnd_500k": MCP4728PwrDown.MCP4728_PD_GND_500KOHM,
}

MCP4728ChannelIdx = mcp4728_ns.enum("MCP4728ChannelIdx")
CHANNEL_OPTIONS = {
    "A": MCP4728ChannelIdx.MCP4728_CHANNEL_A,
    "B": MCP4728ChannelIdx.MCP4728_CHANNEL_B,
    "C": MCP4728ChannelIdx.MCP4728_CHANNEL_C,
    "D": MCP4728ChannelIdx.MCP4728_CHANNEL_D,
}

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(MCP4728Channel),
        cv.GenerateID(CONF_MCP4728_ID): cv.use_id(MCP4728Component),
        cv.Required(CONF_CHANNEL): cv.enum(CHANNEL_OPTIONS, upper=True),
        cv.Optional(CONF_VREF, default="vdd"): cv.enum(VREF_OPTIONS, upper=False),
        cv.Optional(CONF_POWER_DOWN, default="normal"): cv.enum(
            PWRDOWN_OPTIONS, upper=False
        ),
        cv.Optional(CONF_GAIN, default="X1"): cv.enum(GAIN_OPTIONS, upper=True),
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_MCP4728_ID])
    var = cg.new_Pvariable(
        config[CONF_ID],
        paren,
        config[CONF_CHANNEL],
        config[CONF_VREF],
        config[CONF_GAIN],
        config[CONF_POWER_DOWN],
    )
    await output.register_output(var, config)

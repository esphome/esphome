import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_GAIN, CONF_ID
from esphome.types import ConfigType

from .. import CONF_MCP4728_ID, MCP4728Component, mcp4728_ns

DEPENDENCIES = ["mcp4728"]

CONF_VREF = "vref"
CONF_POWER_DOWN = "power_down"

MCP4728Vref = mcp4728_ns.enum("MCP4728Vref")
VREF_OPTIONS = {
    "vdd": MCP4728Vref.MCP4728_VREF_VDD,
    "internal": MCP4728Vref.MCP4728_VREF_INTERNAL_2_8V,
}

MCP4728Gain = mcp4728_ns.enum("MCP4728Gain")
GAIN_OPTIONS = MCP4728Gain.options("X1", "X2", prefix="MCP4728_GAIN_")

MCP4728PwrDown = mcp4728_ns.enum("MCP4728PwrDown")
PWRDOWN_OPTIONS = {
    "normal": MCP4728PwrDown.MCP4728_PD_NORMAL,
    "gnd_1k": MCP4728PwrDown.MCP4728_PD_GND_1KOHM,
    "gnd_100k": MCP4728PwrDown.MCP4728_PD_GND_100KOHM,
    "gnd_500k": MCP4728PwrDown.MCP4728_PD_GND_500KOHM,
}

MCP4728ChannelIdx = mcp4728_ns.enum("MCP4728ChannelIdx")
CHANNEL_OPTIONS = MCP4728ChannelIdx.options(
    "A", "B", "C", "D", prefix="MCP4728_CHANNEL_"
)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.GenerateID(CONF_MCP4728_ID): cv.use_id(MCP4728Component),
        cv.Required(CONF_CHANNEL): cv.enum(CHANNEL_OPTIONS, upper=True),
        cv.Optional(CONF_VREF, default="vdd"): cv.enum(VREF_OPTIONS, upper=False),
        cv.Optional(CONF_POWER_DOWN, default="normal"): cv.enum(
            PWRDOWN_OPTIONS, upper=False
        ),
        cv.Optional(CONF_GAIN, default="X1"): cv.enum(GAIN_OPTIONS, upper=True),
    }
)


async def to_code(config: ConfigType) -> None:
    channel = cg.safe_exp(config[CONF_CHANNEL])
    await output.sub_float_outputs(config, parent=config[CONF_MCP4728_ID]).create(
        config, None, apply=f"write_channel({channel}, state)"
    )
    hub = await cg.get_variable(config[CONF_MCP4728_ID])
    cg.add(
        hub.configure_channel(
            channel,
            config[CONF_VREF],
            config[CONF_GAIN],
            config[CONF_POWER_DOWN],
        )
    )

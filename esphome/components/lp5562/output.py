import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_CURRENT, CONF_ID
from esphome.types import ConfigType

from . import LP5562Output, lp5562_ns

DEPENDENCIES = ["lp5562"]

LP5562Channel = lp5562_ns.class_("LP5562Channel", output.FloatOutput)
CONF_LP5562_ID = "lp5562_id"

# Indices match the LP5562_REG_PWM lookup table in lp5562_output.cpp
LP5562_CHANNELS = {
    "blue": 0,
    "green": 1,
    "red": 2,
    "white": 3,
}


def _current_tenths(amps: float) -> int:
    """Return the current as the register value, in steps of 0.1 mA."""
    tenths = round(amps * 10000)
    if abs(amps * 10000 - tenths) > 1e-6:
        raise cv.Invalid("current must be a multiple of 0.1mA")
    if not 0 <= tenths <= 255:
        raise cv.Invalid("current must be between 0mA and 25.5mA")
    return tenths


CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(LP5562Channel),
        cv.GenerateID(CONF_LP5562_ID): cv.use_id(LP5562Output),
        cv.Required(CONF_CHANNEL): cv.enum(LP5562_CHANNELS, lower=True),
        # The chip's reset value, so a config without it keeps today's behaviour
        cv.Optional(CONF_CURRENT, default="17.5mA"): cv.All(
            cv.current, _current_tenths
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_LP5562_ID])
    parent = await cg.get_variable(config[CONF_LP5562_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(parent.set_channel_current(config[CONF_CHANNEL], config[CONF_CURRENT]))
    await output.register_output(var, config)

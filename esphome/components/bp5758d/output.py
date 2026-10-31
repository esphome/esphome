import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_CURRENT, CONF_ID
from esphome.types import ConfigType

from . import BP5758D

DEPENDENCIES = ["bp5758d"]

CONF_BP5758D_ID = "bp5758d_id"


def _current_register(current: int) -> int:
    """The register value of a current in mA.

    The chip reads bit 6 (normally 64) as 30, so values above 63 are offset by 34:
    64 mA is sent as 98 = 0b01100010, which the chip reads as 30 (bit 6) + 34 (bits 0-5).
    """
    return current if current < 64 else current + 34


CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_BP5758D_ID): cv.use_id(BP5758D),
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=5),
        cv.Optional(CONF_CURRENT, default=10): cv.int_range(min=0, max=90),
    }
)


FINAL_VALIDATE_SCHEMA = output.final_validate_unique_channel(
    platform="bp5758d", hub_key=CONF_BP5758D_ID
)


async def to_code(config: ConfigType) -> None:
    # Channels are numbered from 1 to match OUT1-OUT5; the hub indexes them from 0
    channel = config[CONF_CHANNEL] - 1
    hub = await cg.get_variable(config[CONF_BP5758D_ID])
    await output.sub_float_outputs(config, parent=hub).create(
        config, None, channel, apply="write_channel(index, state)"
    )
    cg.add(hub.set_channel_current(channel, _current_register(config[CONF_CURRENT])))

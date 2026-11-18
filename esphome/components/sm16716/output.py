from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_NUM_CHANNELS
from esphome.types import ConfigType

from . import SM16716

DEPENDENCIES = ["sm16716"]


CONF_SM16716_ID = "sm16716_id"
CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_SM16716_ID): cv.use_id(SM16716),
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=254),
    }
)


FINAL_VALIDATE_SCHEMA = output.final_validate_unique_channel(
    platform="sm16716",
    hub_key=CONF_SM16716_ID,
    # write_channel indexes the hub's num_channels values without a bounds check
    channel_count=lambda hub: hub[CONF_NUM_CHANNELS],
)


async def to_code(config: ConfigType) -> None:
    await output.sub_float_outputs(config, parent=config[CONF_SM16716_ID]).create(
        config, None, config[CONF_CHANNEL], apply="write_channel(index, state)"
    )

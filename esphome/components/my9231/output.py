from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_NUM_CHANNELS
from esphome.types import ConfigType

from . import MY9231OutputComponent

DEPENDENCIES = ["my9231"]


CONF_MY9231_ID = "my9231_id"
CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_MY9231_ID): cv.use_id(MY9231OutputComponent),
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.Required(CONF_CHANNEL): cv.uint16_t,
    }
)


FINAL_VALIDATE_SCHEMA = output.final_validate_unique_channel(
    platform="my9231",
    hub_key=CONF_MY9231_ID,
    # write_channel indexes the hub's num_channels values without a bounds check
    channel_count=lambda hub: hub[CONF_NUM_CHANNELS],
)


async def to_code(config: ConfigType) -> None:
    await output.sub_float_outputs(config, parent=config[CONF_MY9231_ID]).create(
        config, None, config[CONF_CHANNEL], apply="write_channel(index, state)"
    )

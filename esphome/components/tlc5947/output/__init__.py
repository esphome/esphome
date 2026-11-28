from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_NUM_CHIPS
from esphome.types import ConfigType

from .. import CHANNELS_PER_CHIP, TLC5947

DEPENDENCIES = ["tlc5947"]


CONF_TLC5947_ID = "tlc5947_id"
CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_TLC5947_ID): cv.use_id(TLC5947),
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.Required(CONF_CHANNEL): cv.uint16_t,
    }
)


FINAL_VALIDATE_SCHEMA = output.final_validate_unique_channel(
    platform="tlc5947",
    hub_key=CONF_TLC5947_ID,
    channel_count=lambda hub: hub[CONF_NUM_CHIPS] * CHANNELS_PER_CHIP,
)


async def to_code(config: ConfigType) -> None:
    await output.sub_float_outputs(config, parent=config[CONF_TLC5947_ID]).create(
        config, None, config[CONF_CHANNEL], apply="write_channel(index, state)"
    )

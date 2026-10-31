from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_NUM_CHIPS
import esphome.final_validate as fv
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


def _validate_channel_in_range(config: ConfigType) -> ConfigType:
    # The hub only keeps num_chips * 24 duty values and drops writes to any higher channel
    num_chips = fv.get_declaring_config(config[CONF_TLC5947_ID])[CONF_NUM_CHIPS]
    num_channels = num_chips * CHANNELS_PER_CHIP
    if config[CONF_CHANNEL] >= num_channels:
        raise cv.Invalid(
            f"Channel must be less than {num_channels}, the {CHANNELS_PER_CHIP} "
            f"channels of each of the {num_chips} chip(s) of '{config[CONF_TLC5947_ID]}'",
            path=[CONF_CHANNEL],
        )
    return config


FINAL_VALIDATE_SCHEMA = cv.All(
    _validate_channel_in_range,
    output.final_validate_unique_channel(platform="tlc5947", hub_key=CONF_TLC5947_ID),
)


async def to_code(config: ConfigType) -> None:
    await output.sub_float_outputs(config, parent=config[CONF_TLC5947_ID]).create(
        config, None, config[CONF_CHANNEL], apply="write_channel(index, state)"
    )

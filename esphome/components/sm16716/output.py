from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_NUM_CHANNELS
import esphome.final_validate as fv
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


def _validate_channel_in_range(config: ConfigType) -> ConfigType:
    # write_channel indexes a buffer of num_channels entries, so a higher channel writes past it
    num_channels = fv.get_declaring_config(config[CONF_SM16716_ID])[CONF_NUM_CHANNELS]
    if config[CONF_CHANNEL] >= num_channels:
        raise cv.Invalid(
            f"Channel must be less than {num_channels}, the num_channels of "
            f"'{config[CONF_SM16716_ID]}'",
            path=[CONF_CHANNEL],
        )
    return config


FINAL_VALIDATE_SCHEMA = cv.All(
    _validate_channel_in_range,
    output.final_validate_unique_channel(platform="sm16716", hub_key=CONF_SM16716_ID),
)


async def to_code(config: ConfigType) -> None:
    await output.sub_float_outputs(config, parent=config[CONF_SM16716_ID]).create(
        config, None, config[CONF_CHANNEL], apply="write_channel(index, state)"
    )

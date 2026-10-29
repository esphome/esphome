from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_NUM_CHANNELS
import esphome.final_validate as fv
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


def _validate_channel_in_range(config: ConfigType) -> ConfigType:
    # write_channel indexes a buffer of num_channels entries, so a higher channel writes past it
    full_config = fv.full_config.get()
    hub_path = full_config.get_path_for_id(config[CONF_MY9231_ID])[:-1]
    num_channels = full_config.get_config_for_path(hub_path)[CONF_NUM_CHANNELS]
    if config[CONF_CHANNEL] >= num_channels:
        raise cv.Invalid(
            f"Channel must be less than {num_channels}, the num_channels of "
            f"'{config[CONF_MY9231_ID]}'",
            path=[CONF_CHANNEL],
        )
    return config


FINAL_VALIDATE_SCHEMA = cv.All(
    _validate_channel_in_range,
    output.final_validate_unique_channel(platform="my9231", hub_key=CONF_MY9231_ID),
)


async def to_code(config: ConfigType) -> None:
    await output.sub_float_outputs(config, parent=config[CONF_MY9231_ID]).create(
        config, None, config[CONF_CHANNEL], apply="write_channel(index, state)"
    )

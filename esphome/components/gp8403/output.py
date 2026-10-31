from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL
from esphome.types import ConfigType

from . import CONF_GP8403_ID, GP8403Component

DEPENDENCIES = ["gp8403"]


CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(output.ApplyFloatOutput),
        cv.GenerateID(CONF_GP8403_ID): cv.use_id(GP8403Component),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=1),
    }
)


FINAL_VALIDATE_SCHEMA = output.final_validate_unique_channel(
    platform="gp8403", hub_key=CONF_GP8403_ID
)


async def to_code(config: ConfigType) -> None:
    await output.sub_float_outputs(config, parent=config[CONF_GP8403_ID]).create(
        config, None, config[CONF_CHANNEL], apply="write_state(state, index)"
    )

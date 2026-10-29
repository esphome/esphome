from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID
from esphome.types import ConfigType

from . import SM2135

DEPENDENCIES = ["sm2135"]
CODEOWNERS = ["@BoukeHaarsma23"]


CONF_SM2135_ID = "sm2135_id"
CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_SM2135_ID): cv.use_id(SM2135),
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=4),
    }
)


FINAL_VALIDATE_SCHEMA = output.final_validate_unique_channel(
    platform="sm2135", hub_key=CONF_SM2135_ID
)


async def to_code(config: ConfigType) -> None:
    await output.sub_float_outputs(config, parent=config[CONF_SM2135_ID]).create(
        config, None, config[CONF_CHANNEL], apply="write_channel(index, state)"
    )

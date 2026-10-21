from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID
from esphome.types import ConfigType

from . import SM2335

DEPENDENCIES = ["sm2335"]
CODEOWNERS = ["@Cossid"]


CONF_SM2335_ID = "sm2335_id"
CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_SM2335_ID): cv.use_id(SM2335),
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=4),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    await output.sub_float_outputs(config, parent=config[CONF_SM2335_ID]).create(
        config, None, apply=f"write_channel({config[CONF_CHANNEL]}, state)"
    )

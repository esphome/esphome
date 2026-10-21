from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID
from esphome.types import ConfigType

from . import BP1658CJ

DEPENDENCIES = ["bp1658cj"]


CONF_BP1658CJ_ID = "bp1658cj_id"
CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_BP1658CJ_ID): cv.use_id(BP1658CJ),
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=4),
    }
)


async def to_code(config: ConfigType) -> None:
    await output.sub_float_outputs(config, parent=config[CONF_BP1658CJ_ID]).create(
        config, None, config[CONF_CHANNEL], apply="write_channel(index, state)"
    )

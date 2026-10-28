from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_LED
from esphome.types import ConfigType

from ..display import CONF_TM1638_ID, TM1638Component

CONFIG_SCHEMA = output.BINARY_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(output.ApplyBinaryOutput),
        cv.GenerateID(CONF_TM1638_ID): cv.use_id(TM1638Component),
        cv.Required(CONF_LED): cv.int_range(min=0, max=7),
    }
)


async def to_code(config: ConfigType) -> None:
    await output.sub_binary_outputs(config, parent=config[CONF_TM1638_ID]).create(
        config, None, config[CONF_LED], apply="set_led(index, state)"
    )

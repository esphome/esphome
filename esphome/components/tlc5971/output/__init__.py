from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID
from esphome.types import ConfigType

from .. import TLC5971

DEPENDENCIES = ["tlc5971"]


CONF_TLC5971_ID = "tlc5971_id"
CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_TLC5971_ID): cv.use_id(TLC5971),
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.Required(CONF_CHANNEL): cv.uint16_t,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    await output.sub_float_outputs(config, parent=config[CONF_TLC5971_ID]).create(
        config,
        None,
        apply=f"set_channel_value({config[CONF_CHANNEL]}, static_cast<uint16_t>(state * 0xffff))",
    )

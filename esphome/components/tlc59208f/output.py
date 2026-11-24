import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID
from esphome.types import ConfigType

from . import TLC59208FOutput

DEPENDENCIES = ["tlc59208f"]

CONF_TLC59208F_ID = "tlc59208f_id"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.GenerateID(CONF_TLC59208F_ID): cv.use_id(TLC59208FOutput),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
    }
)


FINAL_VALIDATE_SCHEMA = output.final_validate_unique_channel(
    platform="tlc59208f", hub_key=CONF_TLC59208F_ID
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_TLC59208F_ID])
    await output.sub_float_outputs(config, parent=hub).create(
        config, None, config[CONF_CHANNEL], apply="write_channel(index, state)"
    )
    cg.add(hub.register_channel(config[CONF_CHANNEL]))

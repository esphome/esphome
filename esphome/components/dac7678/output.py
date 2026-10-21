import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID
from esphome.types import ConfigType

from . import DAC7678Output

DEPENDENCIES = ["dac7678"]

CONF_DAC7678_ID = "dac7678_id"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.GenerateID(CONF_DAC7678_ID): cv.use_id(DAC7678Output),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DAC7678_ID])
    await output.sub_float_outputs(config, parent=hub).create(
        config, None, config[CONF_CHANNEL], apply="write_channel(index, state)"
    )
    cg.add(hub.register_channel(config[CONF_CHANNEL]))

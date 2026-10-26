import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID
from esphome.types import ConfigType

from . import PCA9685Output

DEPENDENCIES = ["pca9685"]

CONF_PCA9685_ID = "pca9685_id"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.GenerateID(CONF_PCA9685_ID): cv.use_id(PCA9685Output),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=15),
    }
)


FINAL_VALIDATE_SCHEMA = output.final_validate_unique_channel(
    platform="pca9685", hub_key=CONF_PCA9685_ID
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_PCA9685_ID])
    await output.sub_float_outputs(config, parent=hub).create(
        config, None, config[CONF_CHANNEL], apply="write_channel(index, state)"
    )
    cg.add(hub.register_channel(config[CONF_CHANNEL]))

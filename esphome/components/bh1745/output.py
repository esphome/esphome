import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import CONF_BH1745_ID, BH1745Component

DEPENDENCIES = ["bh1745"]


CONFIG_SCHEMA = output.BINARY_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(output.ApplyBinaryOutput),
        cv.GenerateID(CONF_BH1745_ID): cv.use_id(BH1745Component),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BH1745_ID])
    await output.sub_binary_outputs(config, parent=hub).create(
        config, None, apply="set_interrupt_state(state)"
    )
    cg.add(hub.enable_interrupt_output())

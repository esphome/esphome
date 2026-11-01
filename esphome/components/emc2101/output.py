from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from . import CONF_EMC2101_ID, EMC2101_COMPONENT_SCHEMA

DEPENDENCIES = ["emc2101"]


CONFIG_SCHEMA = EMC2101_COMPONENT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
    }
)


async def to_code(config: ConfigType) -> None:
    await output.sub_float_outputs(config, parent=config[CONF_EMC2101_ID]).create(
        config, None, apply="set_duty_cycle(state)"
    )

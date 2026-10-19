from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_LED
from esphome.types import ConfigType

from ..display import CONF_TM1638_ID, TM1638Component

CONFIG_SCHEMA = (
    switch.switch_schema(switch.ApplySwitch)
    .extend(
        {
            cv.GenerateID(CONF_TM1638_ID): cv.use_id(TM1638Component),
            cv.Required(CONF_LED): cv.int_range(min=0, max=7),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    await switch.sub_switches(config, parent=config[CONF_TM1638_ID]).create(
        config, None, apply=f"set_led({config[CONF_LED]}, state)", publish="after"
    )

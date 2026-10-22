import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_DIRECTION, ICON_ROTATE_RIGHT
from esphome.types import ConfigType

from .. import CONF_M5STACK_UNIT_BLDC_ID, M5StackUnitBldc

# The order matches the Direction enum
DIRECTION_OPTIONS = ["Forward", "Backward"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_M5STACK_UNIT_BLDC_ID): cv.use_id(M5StackUnitBldc),
    cv.Optional(CONF_DIRECTION): select.select_schema(
        select.ApplySelect,
        icon=ICON_ROTATE_RIGHT,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_M5STACK_UNIT_BLDC_ID])
    selects = select.sub_selects(config, parent=hub)
    await selects(
        CONF_DIRECTION,
        hub.set_direction_select,
        apply="control_direction(index)",
        options=DIRECTION_OPTIONS,
    )

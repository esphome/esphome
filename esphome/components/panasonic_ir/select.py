import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ICON_ARROW_EXPAND_VERTICAL
from esphome.types import ConfigType

from .climate import PanasonicIrClimate

DEPENDENCIES = ["climate.panasonic_ir"]

CONF_PANASONIC_IR_ID = "panasonic_ir_id"
CONF_VANE_POSITION = "vane_position"

# Option 0 is auto, the others are the fixed positions of the vertical vane from the highest to the lowest
VANE_POSITIONS = ["Auto", "1", "2", "3", "4", "5"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PANASONIC_IR_ID): cv.use_id(PanasonicIrClimate),
        cv.Required(CONF_VANE_POSITION): select.select_schema(
            select.ApplySelect, icon=ICON_ARROW_EXPAND_VERTICAL
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await cg.get_variable(config[CONF_PANASONIC_IR_ID])
    selects = select.sub_selects(config, parent=var)
    await selects(
        CONF_VANE_POSITION,
        var.set_vane_select,
        options=VANE_POSITIONS,
        apply="set_vane_position(index)",
    )

import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_DIRECTION, ICON_ROTATE_RIGHT

from .. import CONF_M5_UNIT_BLDC_ID, M5UnitBldc, m5_unit_bldc_ns

M5UnitBldcDirectionSelect = m5_unit_bldc_ns.class_(
    "M5UnitBldcDirectionSelect", select.Select, cg.Parented.template(M5UnitBldc)
)

DIRECTION_OPTIONS = ["Forward", "Backward"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_M5_UNIT_BLDC_ID): cv.use_id(M5UnitBldc),
    cv.Optional(CONF_DIRECTION): select.select_schema(
        M5UnitBldcDirectionSelect,
        icon=ICON_ROTATE_RIGHT,
    ),
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_M5_UNIT_BLDC_ID])

    if direction_config := config.get(CONF_DIRECTION):
        sel = await select.new_select(direction_config, options=DIRECTION_OPTIONS)
        await cg.register_parented(sel, parent)

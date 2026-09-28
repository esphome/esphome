import esphome.codegen as cg
from esphome.components import select
from esphome.components.const import CONF_OPERATING_MODE
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import CONF_LD2420_ID, LD2420Component, ld2420_ns

CONF_SELECTS = [
    "Normal",
    "Calibrate",
    "Simple",
]

LD2420Select = ld2420_ns.class_("LD2420Select", select.Select, cg.Component)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2420_ID): cv.use_id(LD2420Component),
    cv.Required(CONF_OPERATING_MODE): select.select_schema(
        LD2420Select,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2420_ID])
    selects = select.sub_selects(config, parent=hub)
    await selects(
        CONF_OPERATING_MODE, hub.set_operating_mode_select, options=CONF_SELECTS
    )

import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import CONF_LD2420_ID, LD2420Component

CONF_OPERATING_MODE = "operating_mode"
CONF_SELECTS = [
    "Normal",
    "Calibrate",
    "Simple",
]


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2420_ID): cv.use_id(LD2420Component),
    cv.Required(CONF_OPERATING_MODE): select.select_schema(
        select.ApplySelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2420_ID])
    selects = select.sub_selects(config, parent=hub)
    await selects(
        CONF_OPERATING_MODE,
        hub.set_operating_mode_select,
        options=CONF_SELECTS,
        apply="change_operating_mode(static_cast<ld2420::OpMode>(index + 1))",
    )

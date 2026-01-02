import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_LD2420_ID, LD2420Component, ld2420_ns

CONF_OPERATING_MODE = "operating_mode"
CONF_SELECTS = [
    "Normal",
    "Calibrate",
    "Simple",
]

LD2420Select = ld2420_ns.class_("LD2420Select", cg.Component)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2420_ID): cv.use_id(LD2420Component),
    cv.Required(CONF_OPERATING_MODE): select.select_schema(
        LD2420Select,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
}


async def to_code(config):
    LD2420_component = await cg.get_variable(config[CONF_LD2420_ID])
    if operating_mode_config := config.get(CONF_OPERATING_MODE):
        sel = await select.new_select(
            operating_mode_config,
            options=[CONF_SELECTS],
        )
        await cg.register_parented(sel, config[CONF_LD2420_ID])
        cg.add(LD2420_component.set_operating_mode_select(sel))

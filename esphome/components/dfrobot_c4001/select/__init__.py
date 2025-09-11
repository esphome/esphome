import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_C4001_ID, c4001Component, dfrobot_c4001_ns

CONF_SELECTS = [
    "speed",
    "motion",
]
CONF_OPERATING_MODE = "operating_mode"


C4001Select = dfrobot_c4001_ns.class_("C4001Select", cg.Component)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_C4001_ID): cv.use_id(c4001Component),
    cv.Required(CONF_OPERATING_MODE): select.select_schema(
        C4001Select,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
}


async def to_code(config):
    c4001_component = await cg.get_variable(config[CONF_C4001_ID])
    if operating_mode_config := config.get(CONF_OPERATING_MODE):
        sel = await select.new_select(
            operating_mode_config,
            options=CONF_SELECTS,
        )
        await cg.register_parented(sel, config[CONF_C4001_ID])
        cg.add(c4001_component.set_operating_mode_select(sel))

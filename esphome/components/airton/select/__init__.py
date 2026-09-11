import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from ..climate import (
    CONF_AIRTON_ID,
    CONF_VERTICAL_DIRECTION,
    VERTICAL_DIRECTIONS,
    AirtonClimate,
    airton_ns,
)

CODEOWNERS = ["@procsiab"]

VerticalDirectionSelect = airton_ns.class_("VerticalDirectionSelect", select.Select)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AIRTON_ID): cv.use_id(AirtonClimate),
        cv.Optional(CONF_VERTICAL_DIRECTION): select.select_schema(
            VerticalDirectionSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AIRTON_ID])

    for select_type in [CONF_VERTICAL_DIRECTION]:
        if conf := config.get(select_type):
            sel_var = await select.new_select(
                conf, options=list(VERTICAL_DIRECTIONS.keys())
            )
            await cg.register_parented(sel_var, parent)
            cg.add(getattr(parent, f"set_{select_type}_select")(sel_var))

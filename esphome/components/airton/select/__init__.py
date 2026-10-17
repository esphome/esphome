import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from ..climate import (
    CONF_AIRTON_ID,
    CONF_VERTICAL_DIRECTION,
    VERTICAL_DIRECTIONS,
    AirtonClimate,
    airton_ns,
)

CODEOWNERS = ["@Procsiab"]

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


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_AIRTON_ID])
    selects = select.sub_selects(config, parent=hub)
    await selects(
        CONF_VERTICAL_DIRECTION,
        hub.set_vertical_direction_select,
        options=list(VERTICAL_DIRECTIONS),
    )

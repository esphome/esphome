import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .climate import (
    CONF_AIRTON_ID,
    CONF_VERTICAL_DIRECTION,
    VERTICAL_DIRECTIONS,
    AirtonClimate,
    VerticalDirection,
)

CODEOWNERS = ["@Procsiab"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AIRTON_ID): cv.use_id(AirtonClimate),
        cv.Optional(CONF_VERTICAL_DIRECTION): select.select_schema(
            select.ApplySelect,
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
        # The options are in the order of the enum; the hub publishes the state
        apply=f"control_vertical_direction(static_cast<{VerticalDirection}>(index))",
    )

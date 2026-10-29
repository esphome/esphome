import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from ..climate import (
    CONF_HAIER_ID,
    CONF_HORIZONTAL_AIRFLOW,
    CONF_VERTICAL_AIRFLOW,
    HonClimate,
)

CODEOWNERS = ["@paveldn"]

# Same order as VERTICAL_SWING_MODES_ORDER and HORIZONTAL_SWING_MODES_ORDER in hon_climate.h
AIRFLOW_VERTICAL_DIRECTION_OPTIONS = [
    "Health Up",
    "Max Up",
    "Up",
    "Center",
    "Down",
    "Max Down",
    "Health Down",
]

AIRFLOW_HORIZONTAL_DIRECTION_OPTIONS = [
    "Max Left",
    "Left",
    "Center",
    "Right",
    "Max Right",
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HAIER_ID): cv.use_id(HonClimate),
        cv.Optional(CONF_VERTICAL_AIRFLOW): select.select_schema(
            select.ApplySelect,
            icon="mdi:arrow-expand-vertical",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_HORIZONTAL_AIRFLOW): select.select_schema(
            select.ApplySelect,
            icon="mdi:arrow-expand-horizontal",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_HAIER_ID])
    selects = select.sub_selects(config, parent=hub)
    await selects(
        CONF_VERTICAL_AIRFLOW,
        hub.set_vertical_airflow_select,
        options=AIRFLOW_VERTICAL_DIRECTION_OPTIONS,
        apply="set_vertical_airflow("
        "haier::airflow_order_at(haier::VERTICAL_SWING_MODES_ORDER, index))",
    )
    await selects(
        CONF_HORIZONTAL_AIRFLOW,
        hub.set_horizontal_airflow_select,
        options=AIRFLOW_HORIZONTAL_DIRECTION_OPTIONS,
        apply="set_horizontal_airflow("
        "haier::airflow_order_at(haier::HORIZONTAL_SWING_MODES_ORDER, index))",
    )

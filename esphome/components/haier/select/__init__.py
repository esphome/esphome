import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
)
from ..climate import (
    CONF_HAIER_ID,
    HonClimate,
    haier_ns,
    CONF_VERTICAL_AIRFLOW,
    CONF_HORIZONTAL_AIRFLOW,
)

CODEOWNERS = ["@paveldn"]
VerticalAirflowSelect = haier_ns.class_("VerticalAirflowSelect", select.Select)
HorizontalAirflowSelect = haier_ns.class_("HorizontalAirflowSelect", select.Select)

# Additional icons
ICON_ARROW_HORIZONTAL =  "mdi:arrow-expand-horizontal"
ICON_ARROW_VERTICAL = "mdi:arrow-expand-vertical"

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
            VerticalAirflowSelect,
            icon=ICON_ARROW_VERTICAL,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_HORIZONTAL_AIRFLOW): select.select_schema(
            HorizontalAirflowSelect,
            icon=ICON_ARROW_HORIZONTAL,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)

async def to_code(config):
    parent = await cg.get_variable(config[CONF_HAIER_ID])

    if conf := config.get(CONF_VERTICAL_AIRFLOW):
        sel_var = await select.new_select(conf, options=AIRFLOW_VERTICAL_DIRECTION_OPTIONS)
        await cg.register_parented(sel_var, parent)
        cg.add(getattr(parent, f"set_{CONF_VERTICAL_AIRFLOW}_select")(sel_var))

    if conf := config.get(CONF_HORIZONTAL_AIRFLOW):
        sel_var = await select.new_select(conf, options=AIRFLOW_HORIZONTAL_DIRECTION_OPTIONS)
        await cg.register_parented(sel_var, parent)
        cg.add(getattr(parent, f"set_{CONF_HORIZONTAL_AIRFLOW}_select")(sel_var))



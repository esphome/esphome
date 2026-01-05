import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import Alpha3, alpha3_ns

CONF_ALPHA3_ID = "alpha3_id"

Alpha3Select = alpha3_ns.class_("Alpha3Select", select.Select, cg.Component)

CONF_PUMP_MODE = "pump_mode"

MODE_OPTIONS = [
    "AutoAdapt",
    "Constant Pressure 1 (Min)",
    "Constant Pressure 2",
    "Constant Pressure 3 (Max)",
    "Proportional Pressure 1 (Min)",
    "Proportional Pressure 2",
    "Proportional Pressure 3 (Max)",
    "Constant Frequency",
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ALPHA3_ID): cv.use_id(Alpha3),
        cv.Optional(CONF_PUMP_MODE): select.select_schema(
            Alpha3Select,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ALPHA3_ID])

    if pump_mode_config := config.get(CONF_PUMP_MODE):
        sel = await select.new_select(
            pump_mode_config,
            options=MODE_OPTIONS,
        )
        await cg.register_component(sel, pump_mode_config)
        cg.add(sel.set_parent(parent))

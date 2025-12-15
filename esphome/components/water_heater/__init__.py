import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_TARGET_TEMPERATURE,
    CONF_VISUAL,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority

CODEOWNERS = ["@dhoeben"]

IS_PLATFORM_COMPONENT = True

water_heater_ns = cg.esphome_ns.namespace("water_heater")
WaterHeater = water_heater_ns.class_("WaterHeater", cg.EntityBase, cg.Component)
WaterHeaterCall = water_heater_ns.class_("WaterHeaterCall")
WaterHeaterTraits = water_heater_ns.class_("WaterHeaterTraits")

CONF_TARGET_TEMPERATURE_STEP = "target_temperature_step"
CONF_CURRENT_TEMPERATURE_STEP = "current_temperature_step"

WaterHeaterMode = water_heater_ns.enum("WaterHeaterMode")
WATER_HEATER_MODES = {
    "OFF": WaterHeaterMode.WATER_HEATER_MODE_OFF,
    "ECO": WaterHeaterMode.WATER_HEATER_MODE_ECO,
    "ELECTRIC": WaterHeaterMode.WATER_HEATER_MODE_ELECTRIC,
    "PERFORMANCE": WaterHeaterMode.WATER_HEATER_MODE_PERFORMANCE,
    "HIGH_DEMAND": WaterHeaterMode.WATER_HEATER_MODE_HIGH_DEMAND,
    "HEAT_PUMP": WaterHeaterMode.WATER_HEATER_MODE_HEAT_PUMP,
    "GAS": WaterHeaterMode.WATER_HEATER_MODE_GAS,
}
validate_water_heater_mode = cv.enum(WATER_HEATER_MODES, upper=True)

WATER_HEATER_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(WaterHeater),
        cv.Optional(CONF_VISUAL, default={}): cv.Schema(
            {
                cv.Optional(CONF_MIN_TEMPERATURE): cv.temperature,
                cv.Optional(CONF_MAX_TEMPERATURE): cv.temperature,
                cv.Optional(CONF_TARGET_TEMPERATURE_STEP): cv.float_,
                cv.Optional(CONF_CURRENT_TEMPERATURE_STEP): cv.float_,
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def setup_water_heater_core_(var, config):
    """Setup the core water heater properties in C++."""
    visual = config[CONF_VISUAL]
    if (min_temp := visual.get(CONF_MIN_TEMPERATURE)) is not None:
        cg.add(var.set_visual_min_temperature_override(min_temp))
    if (max_temp := visual.get(CONF_MAX_TEMPERATURE)) is not None:
        cg.add(var.set_visual_max_temperature_override(max_temp))


async def register_water_heater(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)

    cg.add_define("USE_WATER_HEATER")

    await cg.register_component(var, config)

    cg.add(cg.App.register_water_heater(var))

    CORE.register_platform_component("water_heater", var)
    await setup_water_heater_core_(var, config)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(water_heater_ns.using)

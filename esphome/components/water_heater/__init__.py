import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_VISUAL,
    UNIT_CELSIUS,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    queue_entity_register,
    setup_entity,
)
from esphome.cpp_generator import MockObjClass
from esphome.cpp_types import TEMPERATURE_UNITS
from esphome.types import ConfigType

CODEOWNERS = ["@dhoeben"]
DOMAIN = "water_heater"

IS_PLATFORM_COMPONENT = True

water_heater_ns = cg.esphome_ns.namespace("water_heater")
WaterHeater = water_heater_ns.class_("WaterHeater", cg.EntityBase)
WaterHeaterCall = water_heater_ns.class_("WaterHeaterCall")
WaterHeaterTraits = water_heater_ns.class_("WaterHeaterTraits")

CONF_TARGET_TEMPERATURE_STEP = "target_temperature_step"

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


def temperature_unit_of(config: ConfigType) -> str:
    """The unit a water heater reports in: ``unit_of_measurement`` when the platform took the
    option through ``water_heater_schema``, else Celsius."""
    unit = config.get(CONF_UNIT_OF_MEASUREMENT)
    return unit if unit in TEMPERATURE_UNITS else UNIT_CELSIUS


def _visual_temperatures_in_entity_unit(config: ConfigType) -> ConfigType:
    """Convert the visual temperatures into the entity's unit."""
    unit = temperature_unit_of(config)
    visual = config[CONF_VISUAL]
    for key in (CONF_MIN_TEMPERATURE, CONF_MAX_TEMPERATURE):
        if isinstance(value := visual.get(key), tuple):
            visual[key] = cv.convert_temperature(*value, unit)
    if isinstance(value := visual.get(CONF_TARGET_TEMPERATURE_STEP), tuple):
        visual[CONF_TARGET_TEMPERATURE_STEP] = cv.convert_temperature(
            *value, unit, delta=True
        )
    return config


_WATER_HEATER_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.Optional(CONF_VISUAL, default={}): cv.Schema(
            {
                cv.Optional(CONF_MIN_TEMPERATURE): cv.temperature_with_unit,
                cv.Optional(CONF_MAX_TEMPERATURE): cv.temperature_with_unit,
                cv.Optional(CONF_TARGET_TEMPERATURE_STEP): cv.temperature_with_unit,
            }
        ),
    }
)

_WATER_HEATER_SCHEMA.add_extra(entity_duplicate_validator("water_heater"))
_WATER_HEATER_SCHEMA.add_extra(_visual_temperatures_in_entity_unit)


def water_heater_schema(
    class_: MockObjClass,
    *,
    icon: str = cv.UNDEFINED,
    entity_category: str = cv.UNDEFINED,
    temperature_unit: bool = False,
) -> cv.Schema:
    """Build the schema of a water heater platform.

    ``temperature_unit`` adds ``unit_of_measurement`` for platforms whose temperatures are all
    in the unit the user picks; the class then needs ``set_temperature_unit(TemperatureUnit)``.
    """
    schema = {cv.GenerateID(): cv.declare_id(class_)}
    if temperature_unit:
        schema[cv.Optional(CONF_UNIT_OF_MEASUREMENT)] = cv.temperature_unit

    for key, default, validator in [
        (CONF_ICON, icon, cv.icon),
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    return _WATER_HEATER_SCHEMA.extend(schema)


@setup_entity("water_heater")
async def setup_water_heater_core_(var: cg.Pvariable, config: ConfigType) -> None:
    """Set up the core water heater properties in C++."""
    visual = config.get(CONF_VISUAL, {})
    if (min_temp := visual.get(CONF_MIN_TEMPERATURE)) is not None:
        cg.add_define("USE_WATER_HEATER_VISUAL_OVERRIDES")
        cg.add(var.set_visual_min_temperature_override(min_temp))
    if (max_temp := visual.get(CONF_MAX_TEMPERATURE)) is not None:
        cg.add_define("USE_WATER_HEATER_VISUAL_OVERRIDES")
        cg.add(var.set_visual_max_temperature_override(max_temp))
    if (temp_step := visual.get(CONF_TARGET_TEMPERATURE_STEP)) is not None:
        cg.add_define("USE_WATER_HEATER_VISUAL_OVERRIDES")
        cg.add(var.set_visual_target_temperature_step_override(temp_step))


async def register_water_heater(var: cg.Pvariable, config: ConfigType) -> cg.Pvariable:
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)

    cg.add_define("USE_WATER_HEATER")

    queue_entity_register("water_heater", config)

    CORE.register_platform_component("water_heater", var)
    await setup_water_heater_core_(var, config)
    return var


async def new_water_heater(config: ConfigType, *args) -> cg.Pvariable:
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_water_heater(var, config)
    return var


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_global(water_heater_ns.using)

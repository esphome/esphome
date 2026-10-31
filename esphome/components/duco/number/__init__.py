import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, DEVICE_CLASS_TEMPERATURE, UNIT_CELSIUS, UNIT_PERCENT
from esphome.types import ConfigType

from .. import (
    CONF_DUCO_ID,
    DUCO_COMPONENT_SCHEMA,
    DucoDevice,
    duco_ns,
    register_duco_device,
)

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

DucoComfortTemperature = duco_ns.class_(
    "DucoComfortTemperature", DucoDevice, number.Number
)
DucoNightboostTemperature = duco_ns.class_("DucoNightboostTemperature", DucoDevice, number.Number)
DucoNightboostMax = duco_ns.class_("DucoNightboostMax", DucoDevice, number.Number)
DucoBoxVentilationMin = duco_ns.class_("DucoBoxVentilationMin", DucoDevice, number.Number)
DucoBoxVentilationMax = duco_ns.class_("DucoBoxVentilationMax", DucoDevice, number.Number)

CONF_COMFORT_TEMPERATURE = "comfort_temperature"
CONF_NIGHTBOOST_TEMPERATURE = "nightboost_temperature"
CONF_NIGHTBOOST_MAX = "nightboost_max"
CONF_BOX_VENTILATION_MIN = "box_ventilation_min"
CONF_BOX_VENTILATION_MAX = "box_ventilation_max"

COMFORT_TEMPERATURE_SCHEMA = (
    number.number_schema(
        DucoComfortTemperature,
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
    )
    .extend(
        {
            cv.Optional(CONF_ADDRESS, default=0x12): cv.int_range(min=0, max=0xFF),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)

LEGACY_SCHEMA = COMFORT_TEMPERATURE_SCHEMA.extend(DUCO_COMPONENT_SCHEMA)

NIGHTBOOST_TEMPERATURE_SCHEMA = (
    number.number_schema(
        DucoNightboostTemperature,
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ).extend(cv.polling_component_schema("60s"))
)

NIGHTBOOST_MAX_SCHEMA = number.number_schema(DucoNightboostMax, unit_of_measurement=UNIT_PERCENT).extend(
    cv.polling_component_schema("60s")
)

BOX_VENTILATION_MIN_SCHEMA = number.number_schema(DucoBoxVentilationMin, unit_of_measurement=UNIT_PERCENT).extend(
    cv.polling_component_schema("60s")
)

BOX_VENTILATION_MAX_SCHEMA = number.number_schema(DucoBoxVentilationMax, unit_of_measurement=UNIT_PERCENT).extend(
    cv.polling_component_schema("60s")
)

CONFIG_SCHEMA = cv.Any(
    LEGACY_SCHEMA,
    DUCO_COMPONENT_SCHEMA.extend(
        {
            cv.Optional(CONF_COMFORT_TEMPERATURE): cv.ensure_list(COMFORT_TEMPERATURE_SCHEMA),
            cv.Optional(CONF_NIGHTBOOST_TEMPERATURE): NIGHTBOOST_TEMPERATURE_SCHEMA,
            cv.Optional(CONF_NIGHTBOOST_MAX): NIGHTBOOST_MAX_SCHEMA,
            cv.Optional(CONF_BOX_VENTILATION_MIN): BOX_VENTILATION_MIN_SCHEMA,
            cv.Optional(CONF_BOX_VENTILATION_MAX): BOX_VENTILATION_MAX_SCHEMA,
        }
    ),
)


async def to_code(config: ConfigType) -> None:
    grouped_keys = (
        CONF_COMFORT_TEMPERATURE,
        CONF_NIGHTBOOST_TEMPERATURE,
        CONF_NIGHTBOOST_MAX,
        CONF_BOX_VENTILATION_MIN,
        CONF_BOX_VENTILATION_MAX,
    )
    if all(key not in config for key in grouped_keys):
        var = await number.new_number(config, min_value=10.0, max_value=25.5, step=0.1)
        cg.add(var.set_address(config[CONF_ADDRESS]))
        await register_duco_device(var, config, config[CONF_DUCO_ID])
        return

    for entity_config in config.get(CONF_COMFORT_TEMPERATURE, []):
        var = await number.new_number(entity_config, min_value=10.0, max_value=25.5, step=0.1)
        cg.add(var.set_address(entity_config[CONF_ADDRESS]))
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

    if entity_config := config.get(CONF_NIGHTBOOST_TEMPERATURE):
        var = await number.new_number(entity_config, min_value=0, max_value=60, step=1)
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

    if entity_config := config.get(CONF_NIGHTBOOST_MAX):
        var = await number.new_number(entity_config, min_value=10, max_value=100, step=5)
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

    if entity_config := config.get(CONF_BOX_VENTILATION_MIN):
        var = await number.new_number(entity_config, min_value=10, max_value=100, step=5)
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

    if entity_config := config.get(CONF_BOX_VENTILATION_MAX):
        var = await number.new_number(entity_config, min_value=10, max_value=100, step=5)
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

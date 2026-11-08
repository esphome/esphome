import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    DEVICE_CLASS_TEMPERATURE,
    UNIT_CELSIUS,
    UNIT_MINUTE,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)
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
DucoManualTime = duco_ns.class_("DucoManualTime", DucoDevice, number.Number)
DucoManualFlowLevel = duco_ns.class_("DucoManualFlowLevel", DucoDevice, number.Number)
DucoCo2Setpoint = duco_ns.class_("DucoCo2Setpoint", DucoDevice, number.Number)

CONF_COMFORT_TEMPERATURE = "comfort_temperature"
CONF_NIGHTBOOST_TEMPERATURE = "nightboost_temperature"
CONF_NIGHTBOOST_MAX = "nightboost_max"
CONF_BOX_VENTILATION_MIN = "box_ventilation_min"
CONF_BOX_VENTILATION_MAX = "box_ventilation_max"
CONF_MANUAL_TIME = "manual_time"
CONF_FLOW_LEVEL_MANUAL_1 = "flow_level_manual_1"
CONF_FLOW_LEVEL_MANUAL_2 = "flow_level_manual_2"
CONF_FLOW_LEVEL_MANUAL_3 = "flow_level_manual_3"
CONF_CO2_SETPOINT = "co2_setpoint"

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

MANUAL_TIME_SCHEMA = (
    number.number_schema(DucoManualTime, unit_of_measurement=UNIT_MINUTE)
    .extend({cv.Optional(CONF_ADDRESS, default=1): cv.int_range(min=1, max=0x45)})
    .extend(cv.polling_component_schema("60s"))
)

MANUAL_FLOW_LEVEL_SCHEMA = (
    number.number_schema(DucoManualFlowLevel, unit_of_measurement=UNIT_PERCENT)
    .extend({cv.Optional(CONF_ADDRESS, default=1): cv.int_range(min=1, max=0x45)})
    .extend(cv.polling_component_schema("60s"))
)

CO2_SETPOINT_SCHEMA = (
    number.number_schema(DucoCo2Setpoint, unit_of_measurement=UNIT_PARTS_PER_MILLION)
    .extend({cv.Optional(CONF_ADDRESS, default=1): cv.int_range(min=1, max=0x45)})
    .extend(cv.polling_component_schema("60s"))
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
            cv.Optional(CONF_MANUAL_TIME): cv.ensure_list(MANUAL_TIME_SCHEMA),
            cv.Optional(CONF_FLOW_LEVEL_MANUAL_1): cv.ensure_list(MANUAL_FLOW_LEVEL_SCHEMA),
            cv.Optional(CONF_FLOW_LEVEL_MANUAL_2): cv.ensure_list(MANUAL_FLOW_LEVEL_SCHEMA),
            cv.Optional(CONF_FLOW_LEVEL_MANUAL_3): cv.ensure_list(MANUAL_FLOW_LEVEL_SCHEMA),
            cv.Optional(CONF_CO2_SETPOINT): cv.ensure_list(CO2_SETPOINT_SCHEMA),
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
        CONF_MANUAL_TIME,
        CONF_FLOW_LEVEL_MANUAL_1,
        CONF_FLOW_LEVEL_MANUAL_2,
        CONF_FLOW_LEVEL_MANUAL_3,
        CONF_CO2_SETPOINT,
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

    for entity_config in config.get(CONF_MANUAL_TIME, []):
        var = await number.new_number(entity_config, min_value=5, max_value=720, step=5)
        cg.add(var.set_address(entity_config[CONF_ADDRESS]))
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

    for key, item, min_value, max_value in (
        (CONF_FLOW_LEVEL_MANUAL_1, 0x06, 0, 50),
        (CONF_FLOW_LEVEL_MANUAL_2, 0x07, 10, 100),
        (CONF_FLOW_LEVEL_MANUAL_3, 0x08, 50, 100),
    ):
        for entity_config in config.get(key, []):
            var = await number.new_number(entity_config, min_value=min_value, max_value=max_value, step=5)
            cg.add(var.set_address(entity_config[CONF_ADDRESS]))
            cg.add(var.set_item(item))
            await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

    for entity_config in config.get(CONF_CO2_SETPOINT, []):
        var = await number.new_number(entity_config, min_value=0, max_value=2000, step=10)
        cg.add(var.set_address(entity_config[CONF_ADDRESS]))
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

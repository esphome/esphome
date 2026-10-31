import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HUMIDITY,
    CONF_OUTDOOR_TEMPERATURE,
    CONF_POWER,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_GAUGE,
    ICON_HEATING_COIL,
    ICON_PULSE,
    ICON_THERMOMETER,
    ICON_WEATHER_WINDY,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)

from ..climate import CONF_HAIER_ID, HonClimate

CODEOWNERS = ["@paveldn"]
SensorTypeEnum = HonClimate.enum("SubSensorType", True)

# Haier sensors
CONF_COMPRESSOR_CURRENT = "compressor_current"
CONF_COMPRESSOR_FREQUENCY = "compressor_frequency"
CONF_EXPANSION_VALVE_OPEN_DEGREE = "expansion_valve_open_degree"
CONF_INDOOR_COIL_TEMPERATURE = "indoor_coil_temperature"
CONF_OUTDOOR_COIL_TEMPERATURE = "outdoor_coil_temperature"
CONF_OUTDOOR_DEFROST_TEMPERATURE = "outdoor_defrost_temperature"
CONF_OUTDOOR_IN_AIR_TEMPERATURE = "outdoor_in_air_temperature"
CONF_OUTDOOR_OUT_AIR_TEMPERATURE = "outdoor_out_air_temperature"

# Additional icons
ICON_SNOWFLAKE_THERMOMETER = "mdi:snowflake-thermometer"

SENSOR_TYPES = {
    CONF_COMPRESSOR_CURRENT: sensor.sensor_schema(
        kind=sensor.CURRENT,
        accuracy_decimals=1,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_COMPRESSOR_FREQUENCY: sensor.sensor_schema(
        kind=sensor.FREQUENCY,
        icon=ICON_PULSE,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_EXPANSION_VALVE_OPEN_DEGREE: sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        icon=ICON_GAUGE,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_HUMIDITY: sensor.sensor_schema(kind=sensor.HUMIDITY, accuracy_decimals=0),
    CONF_INDOOR_COIL_TEMPERATURE: sensor.sensor_schema(
        kind=sensor.TEMPERATURE,
        icon=ICON_HEATING_COIL,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_OUTDOOR_COIL_TEMPERATURE: sensor.sensor_schema(
        kind=sensor.TEMPERATURE,
        icon=ICON_HEATING_COIL,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_OUTDOOR_DEFROST_TEMPERATURE: sensor.sensor_schema(
        kind=sensor.TEMPERATURE,
        icon=ICON_SNOWFLAKE_THERMOMETER,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_OUTDOOR_IN_AIR_TEMPERATURE: sensor.sensor_schema(
        kind=sensor.TEMPERATURE,
        icon=ICON_WEATHER_WINDY,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_OUTDOOR_OUT_AIR_TEMPERATURE: sensor.sensor_schema(
        kind=sensor.TEMPERATURE,
        icon=ICON_WEATHER_WINDY,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_OUTDOOR_TEMPERATURE: sensor.sensor_schema(
        kind=sensor.TEMPERATURE, icon=ICON_THERMOMETER, accuracy_decimals=0
    ),
    CONF_POWER: sensor.sensor_schema(
        kind=sensor.POWER,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HAIER_ID): cv.use_id(HonClimate),
    }
).extend({cv.Optional(type_): schema for type_, schema in SENSOR_TYPES.items()})


async def to_code(config):
    paren = await cg.get_variable(config[CONF_HAIER_ID])

    for type_ in SENSOR_TYPES:
        if conf := config.get(type_):
            sens = await sensor.new_sensor(conf)
            sensor_type = getattr(SensorTypeEnum, type_.upper())
            cg.add(paren.set_sub_sensor(sensor_type, sens))

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_FAN,
    ICON_RADIATOR,
)
from ..climate import (
    CONF_HAIER_ID,
    HonClimate,
)

CODEOWNERS = ["@paveldn"]
BinarySensorTypeEnum = HonClimate.enum("SubBinarySensorType", True)

# Haier sensors
CONF_OUTDOOR_FAN_STATUS = "outdoor_fan_status"
CONF_DEFROST_STATUS = "defrost_status"
CONF_COMPRESSOR_STATUS = "compressor_status"
CONF_INDOOR_FAN_STATUS = "indoor_fan_status"
CONF_FOUR_WAY_VALVE_STATUS = "four_way_valve_status"
CONF_INDOOR_ELECTRIC_HEATING_STATUS = "indoor_electric_heating_status"

# Additional icons
ICON_SNOWFLAKE_THERMOMETER = "mdi:snowflake-thermometer"
ICON_HVAC = "mdi:hvac"
ICON_VALVE = "mdi:valve"

SENSOR_TYPES = {
    CONF_OUTDOOR_FAN_STATUS: binary_sensor.binary_sensor_schema(
        icon=ICON_FAN,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_DEFROST_STATUS: binary_sensor.binary_sensor_schema(
        icon=ICON_SNOWFLAKE_THERMOMETER,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_COMPRESSOR_STATUS: binary_sensor.binary_sensor_schema(
        icon=ICON_HVAC,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_INDOOR_FAN_STATUS: binary_sensor.binary_sensor_schema(
        icon=ICON_FAN,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_FOUR_WAY_VALVE_STATUS: binary_sensor.binary_sensor_schema(
        icon=ICON_VALVE,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_INDOOR_ELECTRIC_HEATING_STATUS: binary_sensor.binary_sensor_schema(
        icon=ICON_RADIATOR,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HAIER_ID): cv.use_id(HonClimate),
    }
).extend({cv.Optional(type): schema for type, schema in SENSOR_TYPES.items()})


async def to_code(config):
    paren = await cg.get_variable(config[CONF_HAIER_ID])

    for type_ in SENSOR_TYPES:
        if conf := config.get(type_):
            sens = await binary_sensor.new_binary_sensor(conf)
            binary_sensor_type = getattr(BinarySensorTypeEnum, type_.upper())
            cg.add(paren.set_sub_binary_sensor(binary_sensor_type, sens))

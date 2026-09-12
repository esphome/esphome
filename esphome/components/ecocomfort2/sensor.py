import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIRECTION,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TEMPERATURE,
    CONF_VOC,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PARTS_PER_BILLION,
    UNIT_PERCENT,
)

from . import ECOCOMFORT2_CLIENT_SCHEMA, Ecocomfort2Sensor, register_ecocomfort2_child
from .const import (
    CONF_ACTUAL_MODE,
    CONF_ACTUAL_SPEED,
    CONF_HUMIDITY_OFFSET,
    CONF_TEMP_OFFSET,
)

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

SENSOR_TYPES = {
    CONF_TEMPERATURE: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_HUMIDITY: sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_HUMIDITY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_VOC: sensor.sensor_schema(
        unit_of_measurement=UNIT_PARTS_PER_BILLION,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_DIRECTION: sensor.sensor_schema(
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_ACTUAL_MODE: sensor.sensor_schema(
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_ACTUAL_SPEED: sensor.sensor_schema(
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "role": sensor.sensor_schema(
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_TEMP_OFFSET: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_HUMIDITY_OFFSET: sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=2,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Ecocomfort2Sensor),
        }
    )
    .extend({cv.Optional(type_): schema for type_, schema in SENSOR_TYPES.items()})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ECOCOMFORT2_CLIENT_SCHEMA),
    cv.has_at_least_one_key(*SENSOR_TYPES),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_ecocomfort2_child(var, config)

    for key in SENSOR_TYPES:
        if conf := config.get(key):
            sens = await sensor.new_sensor(conf)
            cg.add(getattr(var, f"set_{key}_sensor")(sens))

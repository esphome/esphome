import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIRECTION,
    CONF_HUMIDITY,
    CONF_TEMPERATURE,
    CONF_VOC,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

from . import CONF_ECOCOMFORT2_ID, ECOCOMFORT2_CLIENT_SCHEMA

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

CONFIG_SCHEMA = cv.All(
    ECOCOMFORT2_CLIENT_SCHEMA.extend(
        {
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(kind=sensor.HUMIDITY),
            cv.Optional(CONF_VOC): sensor.sensor_schema(kind=sensor.VOC_PPB),
            cv.Optional(CONF_DIRECTION): sensor.sensor_schema(
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_TEMPERATURE, CONF_HUMIDITY, CONF_VOC, CONF_DIRECTION),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ECOCOMFORT2_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, hub.set_temperature_sensor)
    await sensors(CONF_HUMIDITY, hub.set_humidity_sensor)
    await sensors(CONF_VOC, hub.set_voc_sensor)
    await sensors(CONF_DIRECTION, hub.set_direction_sensor)

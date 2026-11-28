import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_INTERNAL_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

from . import CONF_THERMOPRO_TP970_ID, ThermoProTP970

CONF_AMBIENT_TEMPERATURE = "ambient_temperature"
CONF_NOTIFICATION_COUNT = "notification_count"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_THERMOPRO_TP970_ID): cv.use_id(ThermoProTP970),
        cv.Optional(CONF_INTERNAL_TEMPERATURE): sensor.sensor_schema(
            kind=sensor.TEMPERATURE
        ),
        cv.Optional(CONF_AMBIENT_TEMPERATURE): sensor.sensor_schema(
            kind=sensor.TEMPERATURE
        ),
        cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
            kind=sensor.BATTERY, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_NOTIFICATION_COUNT): sensor.sensor_schema(
            accuracy_decimals=0, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_THERMOPRO_TP970_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_INTERNAL_TEMPERATURE, hub.set_internal_temperature_sensor)
    await sensors(CONF_AMBIENT_TEMPERATURE, hub.set_ambient_temperature_sensor)
    await sensors(CONF_BATTERY_LEVEL, hub.set_battery_level_sensor)
    await sensors(CONF_NOTIFICATION_COUNT, hub.set_notification_count_sensor)

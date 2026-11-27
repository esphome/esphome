import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TARGET_TEMPERATURE
from esphome.types import ConfigType

from .water_heater import CONF_HAIER_WATER_HEATER_ID, HonWaterHeater

CONF_AVAILABLE_WATER = "available_water"
CONF_ESTIMATED_HEATING_TIME = "estimated_heating_time"
CONF_PRIMARY_TEMPERATURE = "primary_temperature"
CONF_SECONDARY_TEMPERATURE = "secondary_temperature"

_TEMPERATURE_SENSOR = sensor.sensor_schema(kind=sensor.TEMPERATURE, accuracy_decimals=0)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HAIER_WATER_HEATER_ID): cv.use_id(HonWaterHeater),
        cv.Optional(CONF_PRIMARY_TEMPERATURE): _TEMPERATURE_SENSOR,
        cv.Optional(CONF_SECONDARY_TEMPERATURE): _TEMPERATURE_SENSOR,
        cv.Optional(CONF_TARGET_TEMPERATURE): _TEMPERATURE_SENSOR,
        cv.Optional(CONF_ESTIMATED_HEATING_TIME): sensor.sensor_schema(
            kind=sensor.DURATION_MIN
        ),
        cv.Optional(CONF_AVAILABLE_WATER): sensor.sensor_schema(kind=sensor.PERCENT),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_HAIER_WATER_HEATER_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_PRIMARY_TEMPERATURE, hub.set_primary_temperature_sensor)
    await sensors(CONF_SECONDARY_TEMPERATURE, hub.set_secondary_temperature_sensor)
    await sensors(CONF_TARGET_TEMPERATURE, hub.set_target_temperature_sensor)
    await sensors(CONF_ESTIMATED_HEATING_TIME, hub.set_estimated_heating_time_sensor)
    await sensors(CONF_AVAILABLE_WATER, hub.set_available_water_sensor)

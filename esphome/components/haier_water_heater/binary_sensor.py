import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_POWER,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_HEAT,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

from .water_heater import CONF_HAIER_WATER_HEATER_ID, HonWaterHeater

CONF_COMMUNICATION = "communication"
CONF_HEATING = "heating"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HAIER_WATER_HEATER_ID): cv.use_id(HonWaterHeater),
        cv.Optional(CONF_COMMUNICATION): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_POWER): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_HEATING): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_HEAT
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_HAIER_WATER_HEATER_ID])
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_COMMUNICATION, hub.set_communication_binary_sensor)
    await binary_sensors(CONF_POWER, hub.set_power_binary_sensor)
    await binary_sensors(CONF_HEATING, hub.set_heating_binary_sensor)

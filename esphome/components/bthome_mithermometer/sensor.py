import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BATTERY_VOLTAGE,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_SIGNAL_STRENGTH,
    CONF_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

from . import bthome_mithermometer_base_schema, setup_bthome_mithermometer

AUTO_LOAD = ["ble_device_base"]

CODEOWNERS = ["@nagyrobi"]

CONFIG_SCHEMA = bthome_mithermometer_base_schema(
    {
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            kind=sensor.TEMPERATURE, accuracy_decimals=2
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            kind=sensor.HUMIDITY, accuracy_decimals=2
        ),
        cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
            kind=sensor.BATTERY,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
            kind=sensor.VOLTAGE,
            accuracy_decimals=3,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_SIGNAL_STRENGTH): sensor.sensor_schema(
            kind=sensor.SIGNAL_STRENGTH,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await setup_bthome_mithermometer(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_HUMIDITY, var.set_humidity_sensor)
    await sensors(CONF_BATTERY_LEVEL, var.set_battery_level_sensor)
    await sensors(CONF_BATTERY_VOLTAGE, var.set_battery_voltage_sensor)
    await sensors(CONF_SIGNAL_STRENGTH, var.set_signal_strength_sensor)

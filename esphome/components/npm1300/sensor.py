import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_VOLT,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_NPM1300_ID, CONF_THERMISTOR_RESISTANCE, NPM1300_CHILD_SCHEMA

DEPENDENCIES = ["npm1300"]

CONF_BATTERY_CURRENT = "battery_current"
CONF_BATTERY_TEMPERATURE = "battery_temperature"
CONF_DIE_TEMPERATURE = "die_temperature"
CONF_SYSTEM_VOLTAGE = "system_voltage"
CONF_VBUS_VOLTAGE = "vbus_voltage"


def _voltage_schema() -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _temperature_schema() -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )


CONFIG_SCHEMA = NPM1300_CHILD_SCHEMA.extend(
    {
        cv.Optional(CONF_BATTERY_VOLTAGE): _voltage_schema(),
        cv.Optional(CONF_BATTERY_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BATTERY_TEMPERATURE): _temperature_schema(),
        cv.Optional(CONF_DIE_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_SYSTEM_VOLTAGE): _voltage_schema(),
        cv.Optional(CONF_VBUS_VOLTAGE): _voltage_schema(),
    }
)


def _final_validate(config: ConfigType) -> None:
    if CONF_BATTERY_TEMPERATURE not in config:
        return
    hub = fv.get_declaring_config(config[CONF_NPM1300_ID])
    if CONF_THERMISTOR_RESISTANCE not in hub:
        raise cv.Invalid(
            f"'{CONF_BATTERY_TEMPERATURE}' needs '{CONF_THERMISTOR_RESISTANCE}' "
            "set on the npm1300 component",
            path=[CONF_BATTERY_TEMPERATURE],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_NPM1300_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_BATTERY_VOLTAGE, hub.set_battery_voltage_sensor)
    await sensors(CONF_BATTERY_CURRENT, hub.set_battery_current_sensor)
    await sensors(CONF_BATTERY_TEMPERATURE, hub.set_battery_temperature_sensor)
    await sensors(CONF_DIE_TEMPERATURE, hub.set_die_temperature_sensor)
    await sensors(CONF_SYSTEM_VOLTAGE, hub.set_system_voltage_sensor)
    await sensors(CONF_VBUS_VOLTAGE, hub.set_vbus_voltage_sensor)

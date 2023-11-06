import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_VOLTAGE,
    CONF_CURRENT,
    CONF_TEMPERATURE,
    UNIT_VOLT,
    UNIT_AMPERE,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_BATTERY,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

from .. import (
    CONF_PYLONTECH_ID,
    PYLONTECH_COMPONENT_SCHEMA,
    CONF_COULOMB,
    CONF_BATTERY,
    CONF_TEMPERATURE_LOW,
    CONF_TEMPERATURE_HIGH,
    CONF_VOLTAGE_HIGH,
    CONF_VOLTAGE_LOW,
    CONF_MOS_TEMPERATURE,
    PYLONTECH_COMPONENT_FINAL_VALIDATE,
)

TYPES: dict[str, cv.Schema] = {
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_CURRENT,
    ),
    CONF_TEMPERATURE: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
    CONF_TEMPERATURE_LOW: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
    CONF_TEMPERATURE_HIGH: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
    CONF_VOLTAGE_LOW: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
    CONF_VOLTAGE_HIGH: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
    CONF_COULOMB: sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_BATTERY,
    ),
    CONF_MOS_TEMPERATURE: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
}

CONFIG_SCHEMA = PYLONTECH_COMPONENT_SCHEMA.extend(
    {cv.Optional(marker): schema for marker, schema in TYPES.items()}
)

FINAL_VALIDATE_SCHEMA = PYLONTECH_COMPONENT_FINAL_VALIDATE


async def to_code(config):
    paren = await cg.get_variable(config[CONF_PYLONTECH_ID])
    bat: int = config[CONF_BATTERY]

    for marker in TYPES:
        if marker in config:
            sens = await sensor.new_sensor(config[marker])
            cg.add(getattr(paren, f"set_{marker}")(sens, bat))

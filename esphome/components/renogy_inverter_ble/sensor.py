"""Sensor platform for renogy_inverter_ble — the inverter's live electrical values."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_VOLT,
    UNIT_WATT,
)

from . import CONF_RENOGY_INVERTER_BLE_ID, RENOGY_INVERTER_BLE_COMPONENT_SCHEMA

DEPENDENCIES = ["renogy_inverter_ble"]
CODEOWNERS = ["@emilioaray-dev"]

UNIT_VOLT_AMPS = "VA"

# (config key, unit, accuracy decimals, device_class) for each measurement the inverter exposes.
SENSORS = [
    ("ac_input_voltage", UNIT_VOLT, 1, DEVICE_CLASS_VOLTAGE),
    ("ac_output_voltage", UNIT_VOLT, 1, DEVICE_CLASS_VOLTAGE),
    ("ac_output_current", UNIT_AMPERE, 2, DEVICE_CLASS_CURRENT),
    ("ac_output_frequency", UNIT_HERTZ, 2, DEVICE_CLASS_FREQUENCY),
    ("input_frequency", UNIT_HERTZ, 2, DEVICE_CLASS_FREQUENCY),
    ("battery_voltage", UNIT_VOLT, 1, DEVICE_CLASS_VOLTAGE),
    ("temperature", UNIT_CELSIUS, 1, DEVICE_CLASS_TEMPERATURE),
    ("load_current", UNIT_AMPERE, 2, DEVICE_CLASS_CURRENT),
    ("load_active_power", UNIT_WATT, 0, DEVICE_CLASS_POWER),
    ("load_apparent_power", UNIT_VOLT_AMPS, 0, DEVICE_CLASS_APPARENT_POWER),
]

CONFIG_SCHEMA = RENOGY_INVERTER_BLE_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(key): sensor.sensor_schema(
            unit_of_measurement=unit,
            accuracy_decimals=decimals,
            device_class=device_class,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        for key, unit, decimals, device_class in SENSORS
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_RENOGY_INVERTER_BLE_ID])
    for key, _unit, _decimals, _device_class in SENSORS:
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(hub, f"set_{key}_sensor")(sens))

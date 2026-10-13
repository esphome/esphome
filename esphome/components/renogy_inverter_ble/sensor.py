import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_VOLTAGE,
    CONF_TEMPERATURE,
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
    UNIT_VOLT_AMPS,
    UNIT_WATT,
)
from esphome.types import ConfigType

from . import CONF_RENOGY_INVERTER_BLE_ID, RENOGY_INVERTER_BLE_COMPONENT_SCHEMA

CODEOWNERS = ["@emilioaray-dev"]
DEPENDENCIES = ["renogy_inverter_ble"]

CONF_AC_INPUT_VOLTAGE = "ac_input_voltage"
CONF_AC_OUTPUT_VOLTAGE = "ac_output_voltage"
CONF_AC_OUTPUT_CURRENT = "ac_output_current"
CONF_AC_OUTPUT_FREQUENCY = "ac_output_frequency"
CONF_INPUT_FREQUENCY = "input_frequency"
CONF_LOAD_CURRENT = "load_current"
CONF_LOAD_ACTIVE_POWER = "load_active_power"
CONF_LOAD_APPARENT_POWER = "load_apparent_power"

VOLTAGE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
)
CURRENT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_AMPERE,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
)
FREQUENCY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_HERTZ,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_FREQUENCY,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = RENOGY_INVERTER_BLE_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(CONF_AC_INPUT_VOLTAGE): VOLTAGE_SCHEMA,
        cv.Optional(CONF_AC_OUTPUT_VOLTAGE): VOLTAGE_SCHEMA,
        cv.Optional(CONF_AC_OUTPUT_CURRENT): CURRENT_SCHEMA,
        cv.Optional(CONF_AC_OUTPUT_FREQUENCY): FREQUENCY_SCHEMA,
        cv.Optional(CONF_INPUT_FREQUENCY): FREQUENCY_SCHEMA,
        cv.Optional(CONF_BATTERY_VOLTAGE): VOLTAGE_SCHEMA,
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LOAD_CURRENT): CURRENT_SCHEMA,
        cv.Optional(CONF_LOAD_ACTIVE_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LOAD_APPARENT_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT_AMPS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_RENOGY_INVERTER_BLE_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_AC_INPUT_VOLTAGE, hub.set_ac_input_voltage_sensor)
    await sensors(CONF_AC_OUTPUT_VOLTAGE, hub.set_ac_output_voltage_sensor)
    await sensors(CONF_AC_OUTPUT_CURRENT, hub.set_ac_output_current_sensor)
    await sensors(CONF_AC_OUTPUT_FREQUENCY, hub.set_ac_output_frequency_sensor)
    await sensors(CONF_INPUT_FREQUENCY, hub.set_input_frequency_sensor)
    await sensors(CONF_BATTERY_VOLTAGE, hub.set_battery_voltage_sensor)
    await sensors(CONF_TEMPERATURE, hub.set_temperature_sensor)
    await sensors(CONF_LOAD_CURRENT, hub.set_load_current_sensor)
    await sensors(CONF_LOAD_ACTIVE_POWER, hub.set_load_active_power_sensor)
    await sensors(CONF_LOAD_APPARENT_POWER, hub.set_load_apparent_power_sensor)

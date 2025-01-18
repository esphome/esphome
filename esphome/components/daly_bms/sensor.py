import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_VOLTAGE,
    CONF_CURRENT,
    CONF_BATTERY_LEVEL,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_PERCENT,
    UNIT_CELSIUS,
    ICON_FLASH,
    ICON_PERCENT,
    ICON_COUNTER,
    ICON_THERMOMETER,
    ICON_GAUGE,
)
from . import DalyBmsComponent, CONF_BMS_DALY_ID

CONF_MAX_CELL_VOLTAGE = "max_cell_voltage"
CONF_MAX_CELL_VOLTAGE_NUMBER = "max_cell_voltage_number"
CONF_MIN_CELL_VOLTAGE = "min_cell_voltage"
CONF_MIN_CELL_VOLTAGE_NUMBER = "min_cell_voltage_number"
CONF_MAX_TEMPERATURE_PROBE_NUMBER = "max_temperature_probe_number"
CONF_MIN_TEMPERATURE_PROBE_NUMBER = "min_temperature_probe_number"
CONF_CELLS_NUMBER = "cells_number"

CONF_REMAINING_CAPACITY = "remaining_capacity"
CONF_TEMPERATURE_1 = "temperature_1"
CONF_TEMPERATURE_2 = "temperature_2"

CONF_CELL_1_VOLTAGE = "cell_1_voltage"
CONF_CELL_2_VOLTAGE = "cell_2_voltage"
CONF_CELL_3_VOLTAGE = "cell_3_voltage"
CONF_CELL_4_VOLTAGE = "cell_4_voltage"
CONF_CELL_5_VOLTAGE = "cell_5_voltage"
CONF_CELL_6_VOLTAGE = "cell_6_voltage"
CONF_CELL_7_VOLTAGE = "cell_7_voltage"
CONF_CELL_8_VOLTAGE = "cell_8_voltage"
CONF_CELL_9_VOLTAGE = "cell_9_voltage"
CONF_CELL_10_VOLTAGE = "cell_10_voltage"
CONF_CELL_11_VOLTAGE = "cell_11_voltage"
CONF_CELL_12_VOLTAGE = "cell_12_voltage"
CONF_CELL_13_VOLTAGE = "cell_13_voltage"
CONF_CELL_14_VOLTAGE = "cell_14_voltage"
CONF_CELL_15_VOLTAGE = "cell_15_voltage"
CONF_CELL_16_VOLTAGE = "cell_16_voltage"
CONF_CELL_17_VOLTAGE = "cell_17_voltage"
CONF_CELL_18_VOLTAGE = "cell_18_voltage"
ICON_CURRENT_DC = "mdi:current-dc"
ICON_BATTERY_OUTLINE = "mdi:battery-outline"
ICON_THERMOMETER_CHEVRON_UP = "mdi:thermometer-chevron-up"
ICON_THERMOMETER_CHEVRON_DOWN = "mdi:thermometer-chevron-down"
ICON_CAR_BATTERY = "mdi:car-battery"

UNIT_AMPERE_HOUR = "Ah"

TYPES = [
    CONF_VOLTAGE,
    CONF_CURRENT,
    CONF_BATTERY_LEVEL,
    CONF_MAX_CELL_VOLTAGE,
    CONF_MAX_CELL_VOLTAGE_NUMBER,
    CONF_MIN_CELL_VOLTAGE,
    CONF_MIN_CELL_VOLTAGE_NUMBER,
    CONF_MAX_TEMPERATURE,
    CONF_MAX_TEMPERATURE_PROBE_NUMBER,
    CONF_MIN_TEMPERATURE,
    CONF_MIN_TEMPERATURE_PROBE_NUMBER,
    CONF_CELLS_NUMBER,
    CONF_REMAINING_CAPACITY,
    CONF_TEMPERATURE_1,
    CONF_TEMPERATURE_2,
    CONF_CELL_1_VOLTAGE,
    CONF_CELL_2_VOLTAGE,
    CONF_CELL_3_VOLTAGE,
    CONF_CELL_4_VOLTAGE,
    CONF_CELL_5_VOLTAGE,
    CONF_CELL_6_VOLTAGE,
    CONF_CELL_7_VOLTAGE,
    CONF_CELL_8_VOLTAGE,
    CONF_CELL_9_VOLTAGE,
    CONF_CELL_10_VOLTAGE,
    CONF_CELL_11_VOLTAGE,
    CONF_CELL_12_VOLTAGE,
    CONF_CELL_13_VOLTAGE,
    CONF_CELL_14_VOLTAGE,
    CONF_CELL_15_VOLTAGE,
    CONF_CELL_16_VOLTAGE,
    CONF_CELL_17_VOLTAGE,
    CONF_CELL_18_VOLTAGE,
]

CELL_VOLTAGE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
    icon=ICON_FLASH,
    accuracy_decimals=3,
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_BMS_DALY_ID): cv.use_id(DalyBmsComponent),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                icon=ICON_FLASH,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                icon=ICON_CURRENT_DC,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MAX_CELL_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                icon=ICON_FLASH,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MAX_CELL_VOLTAGE_NUMBER): sensor.sensor_schema(
                icon=ICON_COUNTER,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_MIN_CELL_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                icon=ICON_FLASH,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MIN_CELL_VOLTAGE_NUMBER): sensor.sensor_schema(
                icon=ICON_COUNTER,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_MAX_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER_CHEVRON_UP,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MAX_TEMPERATURE_PROBE_NUMBER): sensor.sensor_schema(
                icon=ICON_COUNTER,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_MIN_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER_CHEVRON_DOWN,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MIN_TEMPERATURE_PROBE_NUMBER): sensor.sensor_schema(
                icon=ICON_COUNTER,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_REMAINING_CAPACITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE_HOUR,
                icon=ICON_GAUGE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CELLS_NUMBER): sensor.sensor_schema(
                icon=ICON_COUNTER,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_TEMPERATURE_1): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE_2): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CELL_1_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_2_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_3_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_4_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_5_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_6_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_7_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_8_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_9_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_10_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_11_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_12_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_13_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_14_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_15_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_16_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_17_VOLTAGE): CELL_VOLTAGE_SCHEMA,
            cv.Optional(CONF_CELL_18_VOLTAGE): CELL_VOLTAGE_SCHEMA,
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        sens = await sensor.new_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BMS_DALY_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)

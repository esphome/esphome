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
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_NONE,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_PERCENT,
    UNIT_CELSIUS,
    UNIT_EMPTY,
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
]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_BMS_DALY_ID): cv.use_id(DalyBmsComponent),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                UNIT_VOLT,
                ICON_FLASH,
                1,
                DEVICE_CLASS_VOLTAGE,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                UNIT_AMPERE,
                ICON_CURRENT_DC,
                1,
                DEVICE_CLASS_CURRENT,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                UNIT_PERCENT,
                ICON_PERCENT,
                1,
                DEVICE_CLASS_BATTERY,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MAX_CELL_VOLTAGE): sensor.sensor_schema(
                UNIT_VOLT,
                ICON_FLASH,
                2,
                DEVICE_CLASS_VOLTAGE,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MAX_CELL_VOLTAGE_NUMBER): sensor.sensor_schema(
                UNIT_EMPTY,
                ICON_COUNTER,
                0,
                DEVICE_CLASS_EMPTY,
                STATE_CLASS_NONE,
            ),
            cv.Optional(CONF_MIN_CELL_VOLTAGE): sensor.sensor_schema(
                UNIT_VOLT,
                ICON_FLASH,
                2,
                DEVICE_CLASS_VOLTAGE,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MIN_CELL_VOLTAGE_NUMBER): sensor.sensor_schema(
                UNIT_EMPTY,
                ICON_COUNTER,
                0,
                DEVICE_CLASS_EMPTY,
                STATE_CLASS_NONE,
            ),
            cv.Optional(CONF_MAX_TEMPERATURE): sensor.sensor_schema(
                UNIT_CELSIUS,
                ICON_THERMOMETER_CHEVRON_UP,
                0,
                DEVICE_CLASS_TEMPERATURE,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MAX_TEMPERATURE_PROBE_NUMBER): sensor.sensor_schema(
                UNIT_EMPTY,
                ICON_COUNTER,
                0,
                DEVICE_CLASS_EMPTY,
                STATE_CLASS_NONE,
            ),
            cv.Optional(CONF_MIN_TEMPERATURE): sensor.sensor_schema(
                UNIT_CELSIUS,
                ICON_THERMOMETER_CHEVRON_DOWN,
                0,
                DEVICE_CLASS_TEMPERATURE,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MIN_TEMPERATURE_PROBE_NUMBER): sensor.sensor_schema(
                UNIT_EMPTY,
                ICON_COUNTER,
                0,
                DEVICE_CLASS_EMPTY,
                STATE_CLASS_NONE,
            ),
            cv.Optional(CONF_REMAINING_CAPACITY): sensor.sensor_schema(
                UNIT_AMPERE_HOUR,
                ICON_GAUGE,
                2,
                DEVICE_CLASS_VOLTAGE,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CELLS_NUMBER): sensor.sensor_schema(
                UNIT_EMPTY,
                ICON_COUNTER,
                0,
                DEVICE_CLASS_EMPTY,
                STATE_CLASS_NONE,
            ),
            cv.Optional(CONF_TEMPERATURE_1): sensor.sensor_schema(
                UNIT_CELSIUS,
                ICON_THERMOMETER,
                0,
                DEVICE_CLASS_TEMPERATURE,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE_2): sensor.sensor_schema(
                UNIT_CELSIUS,
                ICON_THERMOMETER,
                0,
                DEVICE_CLASS_TEMPERATURE,
                STATE_CLASS_MEASUREMENT,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if key in config:
        conf = config[key]
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BMS_DALY_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)

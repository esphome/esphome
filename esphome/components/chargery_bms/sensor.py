import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_CURRENT,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_IMPEDANCE,
    ICON_COUNTER,
    ICON_FLASH,
    ICON_GAUGE,
    ICON_PERCENT,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_AMPERE_HOUR,
    UNIT_WATT_HOURS,
    UNIT_VOLT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_MILLIOHM,
)
from . import ChargeryBmsComponent, CONF_CHARGERY_BMS_ID

CONF_MAX_CELL_VOLTAGE = "max_cell_voltage"
CONF_MAX_CELL_VOLTAGE_NUMBER = "max_cell_voltage_number"
CONF_MIN_CELL_VOLTAGE = "min_cell_voltage"
CONF_MIN_CELL_VOLTAGE_NUMBER = "min_cell_voltage_number"
CONF_MAX_CELL_IMPEDANCE = "max_cell_impedance"
CONF_MAX_CELL_IMPEDANCE_NUMBER = "max_cell_impedance_number"
CONF_MIN_CELL_IMPEDANCE = "min_cell_impedance"
CONF_MIN_CELL_IMPEDANCE_NUMBER = "min_cell_impedance_number"
CONF_MAX_TEMPERATURE_PROBE_NUMBER = "max_temperature_probe_number"
CONF_MIN_TEMPERATURE_PROBE_NUMBER = "min_temperature_probe_number"
CONF_CELLS_NUMBER = "cells_number"

CONF_REMAINING_CAPACITY_WH = "remaining_capacity_wh"
CONF_REMAINING_CAPACITY_AH = "remaining_capacity_ah"
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
CONF_CELL_19_VOLTAGE = "cell_19_voltage"
CONF_CELL_20_VOLTAGE = "cell_20_voltage"
CONF_CELL_21_VOLTAGE = "cell_21_voltage"
CONF_CELL_22_VOLTAGE = "cell_22_voltage"
CONF_CELL_23_VOLTAGE = "cell_23_voltage"
CONF_CELL_24_VOLTAGE = "cell_24_voltage"
CONF_CELL_1_IMPEDANCE = "cell_1_impedance"
CONF_CELL_2_IMPEDANCE = "cell_2_impedance"
CONF_CELL_3_IMPEDANCE = "cell_3_impedance"
CONF_CELL_4_IMPEDANCE = "cell_4_impedance"
CONF_CELL_5_IMPEDANCE = "cell_5_impedance"
CONF_CELL_6_IMPEDANCE = "cell_6_impedance"
CONF_CELL_7_IMPEDANCE = "cell_7_impedance"
CONF_CELL_8_IMPEDANCE = "cell_8_impedance"
CONF_CELL_9_IMPEDANCE = "cell_9_impedance"
CONF_CELL_10_IMPEDANCE = "cell_10_impedance"
CONF_CELL_11_IMPEDANCE = "cell_11_impedance"
CONF_CELL_12_IMPEDANCE = "cell_12_impedance"
CONF_CELL_13_IMPEDANCE = "cell_13_impedance"
CONF_CELL_14_IMPEDANCE = "cell_14_impedance"
CONF_CELL_15_IMPEDANCE = "cell_15_impedance"
CONF_CELL_16_IMPEDANCE = "cell_16_impedance"
CONF_CELL_17_IMPEDANCE = "cell_17_impedance"
CONF_CELL_18_IMPEDANCE = "cell_18_impedance"
CONF_CELL_19_IMPEDANCE = "cell_19_impedance"
CONF_CELL_20_IMPEDANCE = "cell_20_impedance"
CONF_CELL_21_IMPEDANCE = "cell_21_impedance"
CONF_CELL_22_IMPEDANCE = "cell_22_impedance"
CONF_CELL_23_IMPEDANCE = "cell_23_impedance"
CONF_CELL_24_IMPEDANCE = "cell_24_impedance"
ICON_CURRENT_DC = "mdi:current-dc"
ICON_BATTERY_OUTLINE = "mdi:battery-outline"
ICON_THERMOMETER_CHEVRON_UP = "mdi:thermometer-chevron-up"
ICON_THERMOMETER_CHEVRON_DOWN = "mdi:thermometer-chevron-down"
ICON_CAR_BATTERY = "mdi:car-battery"

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
    CONF_REMAINING_CAPACITY_AH,
    CONF_REMAINING_CAPACITY_WH,
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
    CONF_CELL_19_VOLTAGE,
    CONF_CELL_20_VOLTAGE,
    CONF_CELL_21_VOLTAGE,
    CONF_CELL_22_VOLTAGE,
    CONF_CELL_23_VOLTAGE,
    CONF_CELL_24_VOLTAGE,
    CONF_CELL_1_IMPEDANCE,
    CONF_CELL_2_IMPEDANCE,
    CONF_CELL_3_IMPEDANCE,
    CONF_CELL_4_IMPEDANCE,
    CONF_CELL_5_IMPEDANCE,
    CONF_CELL_6_IMPEDANCE,
    CONF_CELL_7_IMPEDANCE,
    CONF_CELL_8_IMPEDANCE,
    CONF_CELL_9_IMPEDANCE,
    CONF_CELL_10_IMPEDANCE,
    CONF_CELL_11_IMPEDANCE,
    CONF_CELL_12_IMPEDANCE,
    CONF_CELL_13_IMPEDANCE,
    CONF_CELL_14_IMPEDANCE,
    CONF_CELL_15_IMPEDANCE,
    CONF_CELL_16_IMPEDANCE,
    CONF_CELL_17_IMPEDANCE,
    CONF_CELL_18_IMPEDANCE,
    CONF_CELL_19_IMPEDANCE,
    CONF_CELL_20_IMPEDANCE,
    CONF_CELL_21_IMPEDANCE,
    CONF_CELL_22_IMPEDANCE,
    CONF_CELL_23_IMPEDANCE,
    CONF_CELL_24_IMPEDANCE,
]

CELL_VOLTAGE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
    icon=ICON_FLASH,
    accuracy_decimals=3,
)

CELL_IMPEDANCE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_MILLIOHM,
    device_class=DEVICE_CLASS_IMPEDANCE,
    state_class=STATE_CLASS_MEASUREMENT,
    icon=ICON_FLASH,
    accuracy_decimals=1,
)

TEMPERATURE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    icon=ICON_THERMOMETER,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

TEMPERATURE_MIN_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    icon=ICON_THERMOMETER_CHEVRON_DOWN,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

TEMPERATURE_MAX_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    icon=ICON_THERMOMETER_CHEVRON_UP,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)


NUMBER_SCHEMA = sensor.sensor_schema(
    icon=ICON_COUNTER,
    accuracy_decimals=0,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CHARGERY_BMS_ID): cv.use_id(ChargeryBmsComponent),
        cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            icon=ICON_FLASH,
            accuracy_decimals=2,
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
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_BATTERY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_MAX_CELL_VOLTAGE): CELL_VOLTAGE_SCHEMA,
        cv.Optional(CONF_MAX_CELL_VOLTAGE_NUMBER): NUMBER_SCHEMA,
        cv.Optional(CONF_MIN_CELL_VOLTAGE): CELL_VOLTAGE_SCHEMA,
        cv.Optional(CONF_MIN_CELL_VOLTAGE_NUMBER): NUMBER_SCHEMA,
        cv.Optional(CONF_MAX_TEMPERATURE): TEMPERATURE_MAX_SCHEMA,
        cv.Optional(CONF_MAX_TEMPERATURE_PROBE_NUMBER): NUMBER_SCHEMA,
        cv.Optional(CONF_MIN_TEMPERATURE): TEMPERATURE_MIN_SCHEMA,
        cv.Optional(CONF_MIN_TEMPERATURE_PROBE_NUMBER): NUMBER_SCHEMA,
        cv.Optional(CONF_REMAINING_CAPACITY_AH): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE_HOUR,
            icon=ICON_GAUGE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_REMAINING_CAPACITY_WH): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT_HOURS,
            icon=ICON_GAUGE,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CELLS_NUMBER): NUMBER_SCHEMA,
        cv.Optional(CONF_TEMPERATURE_1): TEMPERATURE_SCHEMA,
        cv.Optional(CONF_TEMPERATURE_2): TEMPERATURE_SCHEMA,
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
        cv.Optional(CONF_CELL_19_VOLTAGE): CELL_VOLTAGE_SCHEMA,
        cv.Optional(CONF_CELL_20_VOLTAGE): CELL_VOLTAGE_SCHEMA,
        cv.Optional(CONF_CELL_21_VOLTAGE): CELL_VOLTAGE_SCHEMA,
        cv.Optional(CONF_CELL_22_VOLTAGE): CELL_VOLTAGE_SCHEMA,
        cv.Optional(CONF_CELL_23_VOLTAGE): CELL_VOLTAGE_SCHEMA,
        cv.Optional(CONF_CELL_24_VOLTAGE): CELL_VOLTAGE_SCHEMA,
        cv.Optional(CONF_CELL_1_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_2_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_3_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_4_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_5_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_6_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_7_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_8_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_9_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_10_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_11_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_12_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_13_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_14_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_15_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_16_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_17_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_18_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_19_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_20_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_21_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_22_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_23_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
        cv.Optional(CONF_CELL_24_IMPEDANCE): CELL_IMPEDANCE_SCHEMA,
    }
).extend(cv.COMPONENT_SCHEMA)


async def setup_conf(config, key, hub):
    if key in config:
        conf = config[key]
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_CHARGERY_BMS_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    ICON_EMPTY,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_VOLT,
    UNIT_WATT_HOURS,
    UNIT_WATT,
)

from . import CONF_ABBAURORA_ID, ABBAURORA_COMPONENT_SCHEMA

CONF_V_IN_1 = "v_in_1"
CONF_V_IN_2 = "v_in_2"
CONF_I_IN_1 = "i_in_1"
CONF_I_IN_2 = "i_in_2"
CONF_POWER_IN_1 = "power_in_1"
CONF_POWER_IN_2 = "power_in_2"
CONF_POWER_IN_TOTAL = "power_in_total"
CONF_GRID_POWER = "grid_power"
CONF_TEMPERATURE_INVERTER= "temperature_inverter"
CONF_TEMPERATURE_BOOSTER = "temperature_booster"
CONF_GRID_VOLTAGE = "grid_voltage"
CONF_CUMULATED_ENERGY_TODAY = "cumulated_energy_today" 
CONF_CUMULATED_ENERGY_TOTAL = "cumulated_energy_total"

AUTO_LOAD = ["abbaurora"]

TYPES = {
    CONF_V_IN_1: sensor.sensor_schema(UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_VOLTAGE, STATE_CLASS_MEASUREMENT),
    CONF_V_IN_2: sensor.sensor_schema(UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_VOLTAGE, STATE_CLASS_MEASUREMENT),
    CONF_I_IN_1: sensor.sensor_schema(UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_CURRENT, STATE_CLASS_MEASUREMENT),
    CONF_I_IN_2: sensor.sensor_schema(UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_CURRENT, STATE_CLASS_MEASUREMENT),
    CONF_POWER_IN_1: sensor.sensor_schema(UNIT_WATT, ICON_EMPTY, 0, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT),
    CONF_POWER_IN_2: sensor.sensor_schema(UNIT_WATT, ICON_EMPTY, 0, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT),
    CONF_POWER_IN_TOTAL: sensor.sensor_schema(UNIT_WATT, ICON_EMPTY, 0, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT),
    CONF_GRID_POWER: sensor.sensor_schema(UNIT_WATT, ICON_EMPTY, 0, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT),
    CONF_TEMPERATURE_INVERTER: sensor.sensor_schema(UNIT_CELSIUS, ICON_EMPTY, 1, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT),
    CONF_TEMPERATURE_BOOSTER: sensor.sensor_schema(UNIT_CELSIUS, ICON_EMPTY, 1, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT),
    CONF_GRID_VOLTAGE: sensor.sensor_schema(UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_VOLTAGE, STATE_CLASS_MEASUREMENT),
    CONF_CUMULATED_ENERGY_TODAY: sensor.sensor_schema(UNIT_WATT_HOURS, ICON_EMPTY, 0, DEVICE_CLASS_ENERGY, STATE_CLASS_MEASUREMENT),
    CONF_CUMULATED_ENERGY_TOTAL: sensor.sensor_schema(UNIT_WATT_HOURS, ICON_EMPTY, 0, DEVICE_CLASS_ENERGY, STATE_CLASS_TOTAL_INCREASING),
}

CONFIG_SCHEMA = ABBAURORA_COMPONENT_SCHEMA.extend(
    {cv.Optional(type): schema for type, schema in TYPES.items()}
)

async def to_code(config):
    paren = await cg.get_variable(config[CONF_ABBAURORA_ID])

    for type, _ in TYPES.items():
        if type in config:
            conf = config[type]
            sens = await sensor.new_sensor(conf)
            cg.add(getattr(paren, f"set_{type}_sensor")(sens))

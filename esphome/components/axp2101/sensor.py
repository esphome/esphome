"""Sensor support for AXP2101."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BATTERY_VOLTAGE,
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
)

from . import AXP2101Component, axp2101_ns

DEPENDENCIES = ["axp2101"]

CONF_AXP2101_ID = "axp2101_id"
CONF_VBUS_VOLTAGE = "vbus_voltage"
CONF_VSYS_VOLTAGE = "vsys_voltage"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AXP2101_ID): cv.use_id(AXP2101Component),
        cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_BATTERY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_VBUS_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_VSYS_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    """Generate code for AXP2101 sensors."""
    paren = await cg.get_variable(config[CONF_AXP2101_ID])

    if conf := config.get(CONF_BATTERY_VOLTAGE):
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_battery_voltage_sensor(sens))

    if conf := config.get(CONF_BATTERY_LEVEL):
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_battery_level_sensor(sens))

    if conf := config.get(CONF_VBUS_VOLTAGE):
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_vbus_voltage_sensor(sens))

    if conf := config.get(CONF_VSYS_VOLTAGE):
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_vsys_voltage_sensor(sens))

    if conf := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_die_temperature_sensor(sens))

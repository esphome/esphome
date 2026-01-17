import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_MILLIAMP,
    UNIT_VOLT,
)

from .. import CONF_SY6970_ID, SY6970Component

DEPENDENCIES = ["sy6970"]

CONF_VBUS_VOLTAGE = "vbus_voltage"
CONF_SYSTEM_VOLTAGE = "system_voltage"
CONF_CHARGE_CURRENT = "charge_current"
CONF_PRECHARGE_CURRENT = "precharge_current"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SY6970_ID): cv.use_id(SY6970Component),
        cv.Optional(CONF_VBUS_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SYSTEM_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CHARGE_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIAMP,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PRECHARGE_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIAMP,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SY6970_ID])

    if vbus_voltage_config := config.get(CONF_VBUS_VOLTAGE):
        sens = await sensor.new_sensor(vbus_voltage_config)
        cg.add(parent.set_vbus_voltage_sensor(sens))

    if battery_voltage_config := config.get(CONF_BATTERY_VOLTAGE):
        sens = await sensor.new_sensor(battery_voltage_config)
        cg.add(parent.set_battery_voltage_sensor(sens))

    if system_voltage_config := config.get(CONF_SYSTEM_VOLTAGE):
        sens = await sensor.new_sensor(system_voltage_config)
        cg.add(parent.set_system_voltage_sensor(sens))

    if charge_current_config := config.get(CONF_CHARGE_CURRENT):
        sens = await sensor.new_sensor(charge_current_config)
        cg.add(parent.set_charge_current_sensor(sens))

    if precharge_current_config := config.get(CONF_PRECHARGE_CURRENT):
        sens = await sensor.new_sensor(precharge_current_config)
        cg.add(parent.set_precharge_current_sensor(sens))

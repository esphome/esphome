"""Sensor platform for Sonoff SPM energy monitoring."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_APPARENT_POWER,
    CONF_CURRENT,
    CONF_ENERGY,
    CONF_ID,
    CONF_POWER,
    CONF_POWER_FACTOR,
    CONF_REACTIVE_POWER,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)

from .. import CONF_SONOFF_SPM_ID, SonoffSPM, sonoff_spm_ns

CONF_RELAY_ID = "relay_id"

UNIT_VOLT_AMPERE = "VA"
UNIT_VOLT_AMPERE_REACTIVE = "var"

SonoffSPMSensor = sonoff_spm_ns.class_("SonoffSPMSensor", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SonoffSPMSensor),
        cv.GenerateID(CONF_SONOFF_SPM_ID): cv.use_id(SonoffSPM),
        cv.Required(CONF_RELAY_ID): cv.int_range(min=0, max=127),
        cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_APPARENT_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT_AMPERE,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_REACTIVE_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT_AMPERE_REACTIVE,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_POWER_FACTOR): sensor.sensor_schema(
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ENERGY): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Generate code for Sonoff SPM sensor."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_SONOFF_SPM_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_relay_id(config[CONF_RELAY_ID]))

    if CONF_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_VOLTAGE])
        cg.add(var.set_voltage_sensor(sens))

    if CONF_CURRENT in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT])
        cg.add(var.set_current_sensor(sens))

    if CONF_POWER in config:
        sens = await sensor.new_sensor(config[CONF_POWER])
        cg.add(var.set_power_sensor(sens))

    if CONF_APPARENT_POWER in config:
        sens = await sensor.new_sensor(config[CONF_APPARENT_POWER])
        cg.add(var.set_apparent_power_sensor(sens))

    if CONF_REACTIVE_POWER in config:
        sens = await sensor.new_sensor(config[CONF_REACTIVE_POWER])
        cg.add(var.set_reactive_power_sensor(sens))

    if CONF_POWER_FACTOR in config:
        sens = await sensor.new_sensor(config[CONF_POWER_FACTOR])
        cg.add(var.set_power_factor_sensor(sens))

    if CONF_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_ENERGY])
        cg.add(var.set_energy_sensor(sens))

    cg.add(parent.register_sensor(var, config[CONF_RELAY_ID]))

import logging

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

from .. import CONF_DLMS_METER_ID, CONF_OBIS_CODE, DlmsMeterComponent, obis_code

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["dlms_meter"]

NUMERIC_KEYS = {
    "voltage_l1": "1.0.32.7.0.255",
    "voltage_l2": "1.0.52.7.0.255",
    "voltage_l3": "1.0.72.7.0.255",
    "current_l1": "1.0.31.7.0.255",
    "current_l2": "1.0.51.7.0.255",
    "current_l3": "1.0.71.7.0.255",
    "active_power_plus": "1.0.1.7.0.255",
    "active_power_minus": "1.0.2.7.0.255",
    "active_energy_plus": "1.0.1.8.0.255",
    "active_energy_minus": "1.0.2.8.0.255",
    "reactive_energy_plus": "1.0.3.8.0.255",
    "reactive_energy_minus": "1.0.4.8.0.255",
    "power_factor": "1.0.13.7.0.255",
}

DYNAMIC_SCHEMA = sensor.sensor_schema().extend(
    {
        cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterComponent),
        cv.Required(CONF_OBIS_CODE): obis_code,
    }
)


def deprecation_warning(config):
    _LOGGER.warning(
        "The dlms_meter sensor schema using predefined keys (e.g., 'voltage_l1') is deprecated and will be removed in 2026.11.0. "
        "Please update your configuration to use the new schema with 'obis_code'."
    )
    return config


OLD_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterComponent),
            cv.Optional("voltage_l1"): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional("voltage_l2"): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional("voltage_l3"): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional("current_l1"): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional("current_l2"): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional("current_l3"): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional("active_power_plus"): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional("active_power_minus"): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional("active_energy_plus"): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT_HOURS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional("active_energy_minus"): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT_HOURS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional("reactive_energy_plus"): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT_HOURS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional("reactive_energy_minus"): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT_HOURS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional("power_factor"): sensor.sensor_schema(
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_POWER_FACTOR,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    deprecation_warning,
)


CONFIG_SCHEMA = cv.Any(DYNAMIC_SCHEMA, OLD_SCHEMA)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DLMS_METER_ID])

    if obis := config.get(CONF_OBIS_CODE):
        var = await sensor.new_sensor(config)
        cg.add(hub.register_sensor(obis, var))
    else:
        for key, obis_val in NUMERIC_KEYS.items():
            if sensor_config := config.get(key):
                sens = await sensor.new_sensor(sensor_config)
                cg.add(hub.register_sensor(obis_val, sens))

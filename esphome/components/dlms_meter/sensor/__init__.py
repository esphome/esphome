import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import (
    CONF_DLMS_METER_ID,
    CONF_OBIS_CODE,
    DlmsMeterComponent,
    obis_string_to_byte_list,
    register_sensor,
)

DEPENDENCIES = ["dlms_meter"]

# Removed in 2026.11.0 - kept to provide helpful error message
# Remove before 2027.5.0
_MEASUREMENT = "state_class: measurement"
_VOLTAGE = f"unit_of_measurement: V, accuracy_decimals: 1, device_class: voltage, {_MEASUREMENT}"
_CURRENT = f"unit_of_measurement: A, accuracy_decimals: 2, device_class: current, {_MEASUREMENT}"
_POWER = (
    f"unit_of_measurement: W, accuracy_decimals: 0, device_class: power, {_MEASUREMENT}"
)
_ENERGY = (
    "unit_of_measurement: Wh, accuracy_decimals: 0, device_class: energy, "
    "state_class: total_increasing"
)
_POWER_FACTOR = f"accuracy_decimals: 3, device_class: power_factor, {_MEASUREMENT}"
REMOVED_KEYS = {
    "voltage_l1": ("1.0.32.7.0.255", _VOLTAGE),
    "voltage_l2": ("1.0.52.7.0.255", _VOLTAGE),
    "voltage_l3": ("1.0.72.7.0.255", _VOLTAGE),
    "current_l1": ("1.0.31.7.0.255", _CURRENT),
    "current_l2": ("1.0.51.7.0.255", _CURRENT),
    "current_l3": ("1.0.71.7.0.255", _CURRENT),
    "active_power_plus": ("1.0.1.7.0.255", _POWER),
    "active_power_minus": ("1.0.2.7.0.255", _POWER),
    "active_energy_plus": ("1.0.1.8.0.255", _ENERGY),
    "active_energy_minus": ("1.0.2.8.0.255", _ENERGY),
    "reactive_energy_plus": ("1.0.3.8.0.255", _ENERGY),
    "reactive_energy_minus": ("1.0.4.8.0.255", _ENERGY),
    "power_factor": ("1.0.13.7.0.255", _POWER_FACTOR),
}


CONFIG_SCHEMA = sensor.sensor_schema().extend(
    {
        cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterComponent),
        cv.Required(CONF_OBIS_CODE): obis_string_to_byte_list,
        **{
            cv.Optional(key): cv.invalid(
                f"The predefined '{key}' key was removed in ESPHome 2026.11.0. "
                f"Add a separate '- platform: dlms_meter' sensor with "
                f'obis_code: "{obis}" and {settings}'
            )
            for key, (obis, settings) in REMOVED_KEYS.items()
        },
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DLMS_METER_ID])
    var = await sensor.new_sensor(config)
    register_sensor(hub, config[CONF_OBIS_CODE], var)

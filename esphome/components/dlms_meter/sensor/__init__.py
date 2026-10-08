import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_DLMS_METER_ID, CONF_OBIS_CODE, DlmsMeterComponent, obis_code

DEPENDENCIES = ["dlms_meter"]

# Removed in 2026.11.0 - kept to provide helpful error message
# Remove before 2027.5.0
REMOVED_KEYS = {
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


CONFIG_SCHEMA = sensor.sensor_schema().extend(
    {
        cv.GenerateID(CONF_DLMS_METER_ID): cv.use_id(DlmsMeterComponent),
        cv.Required(CONF_OBIS_CODE): obis_code,
        **{
            cv.Optional(key): cv.invalid(
                f"The predefined '{key}' key was removed in ESPHome 2026.11.0. "
                f"Use 'obis_code: \"{obis}\"' and set the unit and classes yourself"
            )
            for key, obis in REMOVED_KEYS.items()
        },
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DLMS_METER_ID])
    var = await sensor.new_sensor(config)
    cg.add(hub.register_sensor(config[CONF_OBIS_CODE], var))

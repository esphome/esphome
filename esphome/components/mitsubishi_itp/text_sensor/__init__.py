from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.core import coroutine

from ...mitsubishi_itp import (
    mitsubishi_itp_ns,
    sensors_to_code,
    sensors_to_config_schema,
)

CONF_THERMOSTAT_BATTERY = "thermostat_battery"

CONF_ERROR_CODE = "error_code"

ActualFanSensor = mitsubishi_itp_ns.class_("ActualFanSensor", text_sensor.TextSensor)
ErrorCodeSensor = mitsubishi_itp_ns.class_("ErrorCodeSensor", text_sensor.TextSensor)
ThermostatBatterySensor = mitsubishi_itp_ns.class_(
    "ThermostatBatterySensor", text_sensor.TextSensor
)

# TODO Storing the registration function here seems weird, but I can't figure out how to determine schema type later
SENSORS = dict[str, cv.Schema](
    {
        "actual_fan": text_sensor.text_sensor_schema(
            ActualFanSensor,
            icon="mdi:fan",
        ),
        CONF_ERROR_CODE: text_sensor.text_sensor_schema(
            ErrorCodeSensor, icon="mdi:alert-circle-outline"
        ),
        CONF_THERMOSTAT_BATTERY: text_sensor.text_sensor_schema(
            ThermostatBatterySensor,
            icon="mdi:battery",
        ),
    }
)

CONFIG_SCHEMA = sensors_to_config_schema(SENSORS)


@coroutine
async def to_code(config):
    await sensors_to_code(config, SENSORS, text_sensor.register_text_sensor)

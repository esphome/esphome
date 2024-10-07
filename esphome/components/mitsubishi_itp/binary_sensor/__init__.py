from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.core import coroutine

from ...mitsubishi_itp import (
    mitsubishi_itp_ns,
    sensors_to_code,
    sensors_to_config_schema,
)

CONF_ISEE_STATUS = "isee_status"


DefrostSensor = mitsubishi_itp_ns.class_("DefrostSensor", binary_sensor.BinarySensor)
FilterStatusSensor = mitsubishi_itp_ns.class_(
    "FilterStatusSensor", binary_sensor.BinarySensor
)
ISeeStatusSensor = mitsubishi_itp_ns.class_(
    "ISeeStatusSensor", binary_sensor.BinarySensor
)
PreheatSensor = mitsubishi_itp_ns.class_("PreheatSensor", binary_sensor.BinarySensor)
StandbySensor = mitsubishi_itp_ns.class_("StandbySensor", binary_sensor.BinarySensor)

# TODO Storing the registration function here seems weird, but I can't figure out how to determine schema type later
SENSORS = dict[str, cv.Schema](
    {
        "defrost": binary_sensor.binary_sensor_schema(
            DefrostSensor, icon="mdi:snowflake-melt"
        ),
        "filter_status": binary_sensor.binary_sensor_schema(
            FilterStatusSensor, device_class="problem", icon="mdi:air-filter"
        ),
        CONF_ISEE_STATUS: binary_sensor.binary_sensor_schema(
            ISeeStatusSensor, icon="mdi:eye"
        ),
        "preheat": binary_sensor.binary_sensor_schema(
            PreheatSensor, icon="mdi:heating-coil"
        ),
        "standby": binary_sensor.binary_sensor_schema(
            StandbySensor, icon="mdi:pause-circle-outline"
        ),
    }
)

CONFIG_SCHEMA = sensors_to_config_schema(SENSORS)


@coroutine
async def to_code(config):
    await sensors_to_code(config, SENSORS, binary_sensor.register_binary_sensor)

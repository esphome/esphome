import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCELERATION_X,
    CONF_ACCELERATION_Y,
    CONF_ACCELERATION_Z,
    CONF_NAME,
    ICON_BRIEFCASE_DOWNLOAD,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER_PER_SECOND_SQUARED,
)

from . import CONF_MSA3XX_ID, MSA_SENSOR_SCHEMA

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["msa3xx"]

ACCELERATION_SENSORS = (CONF_ACCELERATION_X, CONF_ACCELERATION_Y, CONF_ACCELERATION_Z)

accel_schema = cv.maybe_simple_value(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_METER_PER_SECOND_SQUARED,
        icon=ICON_BRIEFCASE_DOWNLOAD,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    key=CONF_NAME,
)


CONFIG_SCHEMA = MSA_SENSOR_SCHEMA.extend(
    {cv.Optional(sensor): accel_schema for sensor in ACCELERATION_SENSORS}
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MSA3XX_ID])
    for accel_key in ACCELERATION_SENSORS:
        if accel_key in config:
            sens = await sensor.new_sensor(config[accel_key])
            cg.add(getattr(hub, f"set_{accel_key}_sensor")(sens))

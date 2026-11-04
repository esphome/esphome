import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCELERATION_X,
    CONF_ACCELERATION_Y,
    CONF_ACCELERATION_Z,
    CONF_NAME,
    ICON_ACCELERATION,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER_PER_SECOND_SQUARED,
)
from esphome.types import ConfigType

from . import CONF_LIS2DW12_ID, LIS2DW12_SENSOR_SCHEMA

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["lis2dw12"]

ACCELERATION_SENSORS = (CONF_ACCELERATION_X, CONF_ACCELERATION_Y, CONF_ACCELERATION_Z)

accel_schema = cv.maybe_simple_value(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_METER_PER_SECOND_SQUARED,
        icon=ICON_ACCELERATION,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    key=CONF_NAME,
)


CONFIG_SCHEMA = LIS2DW12_SENSOR_SCHEMA.extend(
    {cv.Optional(key): accel_schema for key in ACCELERATION_SENSORS}
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LIS2DW12_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_ACCELERATION_X, hub.set_acceleration_x_sensor)
    await sensors(CONF_ACCELERATION_Y, hub.set_acceleration_y_sensor)
    await sensors(CONF_ACCELERATION_Z, hub.set_acceleration_z_sensor)

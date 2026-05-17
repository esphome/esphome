import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCELERATION_X,
    CONF_ACCELERATION_Y,
    CONF_ACCELERATION_Z,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_ACCELERATION_X,
    ICON_ACCELERATION_Y,
    ICON_ACCELERATION_Z,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_METER_PER_SECOND_SQUARED,
)

from . import CONF_LIS3DH_ID, LIS3DHComponent


def _accel_schema(icon):
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_METER_PER_SECOND_SQUARED,
        icon=icon,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LIS3DH_ID): cv.use_id(LIS3DHComponent),
        cv.Optional(CONF_ACCELERATION_X): _accel_schema(ICON_ACCELERATION_X),
        cv.Optional(CONF_ACCELERATION_Y): _accel_schema(ICON_ACCELERATION_Y),
        cv.Optional(CONF_ACCELERATION_Z): _accel_schema(ICON_ACCELERATION_Z),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LIS3DH_ID])

    if axis_conf := config.get(CONF_ACCELERATION_X):
        cg.add(hub.set_acceleration_x_sensor(await sensor.new_sensor(axis_conf)))
    if axis_conf := config.get(CONF_ACCELERATION_Y):
        cg.add(hub.set_acceleration_y_sensor(await sensor.new_sensor(axis_conf)))
    if axis_conf := config.get(CONF_ACCELERATION_Z):
        cg.add(hub.set_acceleration_z_sensor(await sensor.new_sensor(axis_conf)))
    if temp_conf := config.get(CONF_TEMPERATURE):
        cg.add(hub.set_temperature_sensor(await sensor.new_sensor(temp_conf)))

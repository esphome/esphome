# ── YAML config keys ─────────────────────────────────────────────────────────
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_TEMPERATURE,
    CONF_TYPE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_ACCELERATION,
    ICON_ROTATE_RIGHT,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_DEGREE_PER_SECOND,
    UNIT_G,
)
from esphome.cpp_generator import MockObj

from . import CONF_BMI270_ID, SENSOR_SCHEMA, BMI270AccelData

CONF_ACCEL_X = "accel_x"
CONF_ACCEL_Y = "accel_y"
CONF_ACCEL_Z = "accel_z"
CONF_GYRO_X = "gyro_x"
CONF_GYRO_Y = "gyro_y"
CONF_GYRO_Z = "gyro_z"


# ── Sensor schema helpers ────────────────────────────────────────────────────
def _accel_sensor_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_G,
        icon=ICON_ACCELERATION,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(SENSOR_SCHEMA)


def _gyro_sensor_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_DEGREE_PER_SECOND,
        icon=ICON_ROTATE_RIGHT,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(SENSOR_SCHEMA)


CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_ACCEL_X: _accel_sensor_schema(),
        CONF_ACCEL_Y: _accel_sensor_schema(),
        CONF_ACCEL_Z: _accel_sensor_schema(),
        # Gyroscope axes
        CONF_GYRO_X: _gyro_sensor_schema(),
        CONF_GYRO_Y: _gyro_sensor_schema(),
        CONF_GYRO_Z: _gyro_sensor_schema(),
        # Temperature
        CONF_TEMPERATURE: sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon=ICON_THERMOMETER,
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
            device_class=DEVICE_CLASS_TEMPERATURE,
        ).extend(SENSOR_SCHEMA),
    }
)


async def to_code(config):
    type = config[CONF_TYPE]
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_BMI270_ID])
    data = MockObj("data")
    value_lambda = await cg.process_lambda(
        var.publish_state(getattr(data, type)),
        [(BMI270AccelData.operator("ref"), "data")],
    )
    cg.add(parent.add_listener(value_lambda))

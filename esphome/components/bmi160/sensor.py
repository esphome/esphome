import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCELERATION_X,
    CONF_ACCELERATION_Y,
    CONF_ACCELERATION_Z,
    CONF_GYROSCOPE_X,
    CONF_GYROSCOPE_Y,
    CONF_GYROSCOPE_Z,
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_ACCELERATION_X,
    ICON_ACCELERATION_Y,
    ICON_ACCELERATION_Z,
    ICON_GYROSCOPE_X,
    ICON_GYROSCOPE_Y,
    ICON_GYROSCOPE_Z,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_DEGREE_PER_SECOND,
    UNIT_METER_PER_SECOND_SQUARED,
)
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

bmi160_ns = cg.esphome_ns.namespace("bmi160")
BMI160Component = bmi160_ns.class_(
    "BMI160Component", cg.PollingComponent, i2c.I2CDevice
)

accel_schema = {
    "unit_of_measurement": UNIT_METER_PER_SECOND_SQUARED,
    "accuracy_decimals": 2,
    "state_class": STATE_CLASS_MEASUREMENT,
}
gyro_schema = {
    "unit_of_measurement": UNIT_DEGREE_PER_SECOND,
    "accuracy_decimals": 2,
    "state_class": STATE_CLASS_MEASUREMENT,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BMI160Component),
            cv.Optional(CONF_ACCELERATION_X): sensor.sensor_schema(
                icon=ICON_ACCELERATION_X,
                **accel_schema,
            ),
            cv.Optional(CONF_ACCELERATION_Y): sensor.sensor_schema(
                icon=ICON_ACCELERATION_Y,
                **accel_schema,
            ),
            cv.Optional(CONF_ACCELERATION_Z): sensor.sensor_schema(
                icon=ICON_ACCELERATION_Z,
                **accel_schema,
            ),
            cv.Optional(CONF_GYROSCOPE_X): sensor.sensor_schema(
                icon=ICON_GYROSCOPE_X,
                **gyro_schema,
            ),
            cv.Optional(CONF_GYROSCOPE_Y): sensor.sensor_schema(
                icon=ICON_GYROSCOPE_Y,
                **gyro_schema,
            ),
            cv.Optional(CONF_GYROSCOPE_Z): sensor.sensor_schema(
                icon=ICON_GYROSCOPE_Z,
                **gyro_schema,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x68))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_ACCELERATION_X, var.set_accel_x_sensor)
    await sensors(CONF_GYROSCOPE_X, var.set_gyro_x_sensor)
    await sensors(CONF_ACCELERATION_Y, var.set_accel_y_sensor)
    await sensors(CONF_GYROSCOPE_Y, var.set_gyro_y_sensor)
    await sensors(CONF_ACCELERATION_Z, var.set_accel_z_sensor)
    await sensors(CONF_GYROSCOPE_Z, var.set_gyro_z_sensor)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)

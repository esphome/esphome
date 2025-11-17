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

DEPENDENCIES = ["i2c"]

bmi270_ns = cg.esphome_ns.namespace("bmi270")
BMI270Component = bmi270_ns.class_(
    "BMI270Component", cg.PollingComponent, i2c.I2CDevice
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
            cv.GenerateID(): cv.declare_id(BMI270Component),
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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for d in ["x", "y", "z"]:
        accel_key = f"acceleration_{d}"
        if accel_conf := config.get(accel_key):
            sens = await sensor.new_sensor(accel_conf)
            cg.add(getattr(var, f"set_accel_{d}_sensor")(sens))
        gyro_key = f"gyroscope_{d}"
        if gyro_conf := config.get(gyro_key):
            sens = await sensor.new_sensor(gyro_conf)
            cg.add(getattr(var, f"set_gyro_{d}_sensor")(sens))

    if temp_conf := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temp_conf)
        cg.add(var.set_temperature_sensor(sens))

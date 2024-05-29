import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_INTERRUPT, CONF_THRESHOLD, CONF_DURATION

CONF_MPU6050_ID = "mpu6050_id"

mpu6050_ns = cg.esphome_ns.namespace("mpu6050")
MPU6050Component = mpu6050_ns.class_("MPU6050Component", cg.Component)


def validate_interrupt(value):
    schema = None
    if isinstance(value, bool):
        if value:
            schema = {CONF_THRESHOLD: 1, CONF_DURATION: cv.positive_time_period("1ms")}

    else:
        schema = cv.Schema(
            {
                cv.Required(CONF_THRESHOLD): cv.positive_not_null_int,
                cv.Required(CONF_DURATION): cv.positive_time_period,
            }
        )(value)
    return schema


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MPU6050Component),
        cv.Optional(CONF_INTERRUPT, default=False): validate_interrupt,
    }
).extend(i2c.i2c_device_schema(0x68))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    if conf_interrupt := config[CONF_INTERRUPT]:
        if conf_interrupt:
            cg.add_define("USE_MPU6050_INTERRUPT")
            cg.add(
                var.set_interrupt(
                    conf_interrupt[CONF_THRESHOLD],
                    conf_interrupt[CONF_DURATION].total_milliseconds,
                )
            )

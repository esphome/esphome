import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CONF_INTERRUPT = "interrupt"
CONF_THRESHOLD = "threshold"
CONF_DURATION = "duration"

mpu6050_ns = cg.esphome_ns.namespace("mpu6050")
MPU6050Component = mpu6050_ns.class_("MPU6050Component", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MPU6050Component),
        cv.Optional(CONF_INTERRUPT): cv.Schema(
            {
                cv.Required(CONF_THRESHOLD): cv.positive_not_null_int,
                cv.Required(CONF_DURATION): cv.TimePeriodMilliseconds,
            }
        ),
    }
).extend(i2c.i2c_device_schema(0x68))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if CONF_INTERRUPT in config:
        cg.add_define("USE_MPU6050_INTERRUPT")
        interrupt = config[CONF_INTERRUPT]
        cg.add(var.set_interrupt(interrupt[CONF_THRESHOLD], interrupt[CONF_DURATION]))

    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

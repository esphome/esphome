import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID


DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@rnauber"]
MULTI_CONF = True

CONF_M5STACK_8ANGLE_ID = "m5stack_8angle_id"

m5stack_8angle_ns = cg.esphome_ns.namespace("m5stack_8angle")
M5Stack8AngleComponent = m5stack_8angle_ns.class_(
    "M5Stack8AngleComponent",
    i2c.I2CDevice,
    cg.Component,
)

AnalogBits = m5stack_8angle_ns.enum("AnalogBits")


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(M5Stack8AngleComponent),
    }
).extend(i2c.i2c_device_schema(0x43))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

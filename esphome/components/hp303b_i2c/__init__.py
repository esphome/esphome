import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, hp303b
from esphome.const import CONF_ID

AUTO_LOAD = ["hp303b"]
CODEOWNERS = ["@max246"]
DEPENDENCIES = ["i2c"]

hp303b_i2c_ns = cg.esphome_ns.namespace("hp303b_i2c")
HP303BComponentI2C = hp303b_i2c_ns.class_(
    "HP303BComponentI2C", hp303b.HP303BComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = cv.All(
    hp303b.CONFIG_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HP303BComponentI2C),
        }
    ).extend(i2c.i2c_device_schema(0x77))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await i2c.register_i2c_device(var, config)

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, ade7880_base
from esphome.const import CONF_ID


DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["ade7880_base"]

ade7880_ns = cg.esphome_ns.namespace("ade7880_i2c")
ADE7880 = ade7880_ns.class_("ADE7880I2C", ade7880_base.ADE7880, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADE7880),
        }
    )
    .extend(ade7880_base.ADE7880_CONFIG_SCHEMA)
    .extend(i2c.i2c_device_schema(0x38))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await i2c.register_i2c_device(var, config)
    await ade7880_base.register_ade7880(var, config)

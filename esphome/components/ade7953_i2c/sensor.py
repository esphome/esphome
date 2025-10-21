import esphome.codegen as cg
from esphome.components import ade7953_base, i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["ade7953_base"]

ade7953_ns = cg.esphome_ns.namespace("ade7953_i2c")
ADE7953 = ade7953_ns.class_("AdE7953I2c", ade7953_base.ADE7953, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADE7953),
        }
    )
    .extend(ade7953_base.ADE7953_CONFIG_SCHEMA)
    .extend(i2c.i2c_device_schema(0x38))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await i2c.register_i2c_device(var, config)
    await ade7953_base.register_ade7953(var, config)

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, wk2168
from esphome.const import CONF_ID

CODEOWNERS = ["@DrCoolZic"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["wk2168"]
MULTI_CONF = True

wk2168_ns = cg.esphome_ns.namespace("wk2168_i2c")
WK2168ComponentI2C = wk2168_ns.class_(
    "WK2168ComponentI2C", wk2168.WK2168Component, i2c.I2CDevice
)

CONFIG_SCHEMA = cv.All(
    wk2168.WK2168_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WK2168ComponentI2C),
        }
    ).extend(i2c.i2c_device_schema(0x2C)),
    wk2168.post_check_conf_wk2168,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add_build_flag("-DI2C_BUFFER_LENGTH=255")
    cg.add_define("I2C_COMPILE")  # add to defines.h
    cg.add(var.set_name(str(config[CONF_ID])))
    await wk2168.register_wk2168(var, config)
    await i2c.register_i2c_device(var, config)

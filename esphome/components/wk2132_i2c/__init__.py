import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, wk2132
from esphome.const import CONF_ID

CODEOWNERS = ["@DrCoolZic"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["wk2132"]
MULTI_CONF = True

wk2132_ns = cg.esphome_ns.namespace("wk2132_i2c")
WK2132ComponentI2C = wk2132_ns.class_(
    "WK2132ComponentI2C", wk2132.WK2132Component, i2c.I2CDevice
)

CONFIG_SCHEMA = cv.All(
    wk2132.WK2132_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WK2132ComponentI2C),
        }
    ).extend(i2c.i2c_device_schema(0x2C)),
    wk2132.post_check_conf_wk2132,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add_build_flag("-DI2C_BUFFER_LENGTH=255")
    cg.add_define("I2C_COMPILE")  # add to defines.h
    cg.add(var.set_name(str(config[CONF_ID])))
    await wk2132.register_wk2132(var, config)
    await i2c.register_i2c_device(var, config)

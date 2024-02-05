import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, wk_base
from esphome.const import CONF_ID

CODEOWNERS = ["@DrCoolZic"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["wk_base"]
MULTI_CONF = True

wk_base_ns = cg.esphome_ns.namespace("wk_base")
WKBaseComponentI2C = wk_base_ns.class_(
    "WKBaseComponentI2C", wk_base.WKBaseComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = cv.All(
    wk_base.WKBASE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WKBaseComponentI2C),
        }
    ).extend(i2c.i2c_device_schema(0x2C)),
    wk_base.check_channel_max_4,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add_build_flag("-DI2C_BUFFER_LENGTH=255")
    cg.add_define("USE_I2C_BUS")  # add to defines.h
    cg.add(var.set_name(str(config[CONF_ID])))
    await wk_base.register_wk_base(var, config)
    await i2c.register_i2c_device(var, config)

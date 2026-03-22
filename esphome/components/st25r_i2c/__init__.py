import esphome.codegen as cg
from esphome.components import st25r, i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["st25r"]
CODEOWNERS = ["@JohnMcLear"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

st25r_i2c_ns = cg.esphome_ns.namespace("st25r_i2c")
ST25RI2c = st25r_i2c_ns.class_("ST25RI2c", st25r.ST25R, i2c.I2CDevice)

CONFIG_SCHEMA = cv.All(
    st25r.ST25R_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ST25RI2c),
        }
    ).extend(i2c.i2c_device_schema(0x50))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await st25r.setup_st25r(var, config)
    await i2c.register_i2c_device(var, config)

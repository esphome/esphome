import esphome.codegen as cg
from esphome.components import i2c, st7567_base
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_PAGES

CODEOWNERS = ["@latonita"]

AUTO_LOAD = ["st7567_base"]
DEPENDENCIES = ["i2c"]

st7567_i2c = cg.esphome_ns.namespace("st7567_i2c")
I2CST7567 = st7567_i2c.class_("I2CST7567", st7567_base.ST7567, i2c.I2CDevice)

CONFIG_SCHEMA = cv.All(
    st7567_base.ST7567_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(I2CST7567),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x3F)),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await st7567_base.setup_st7567(var, config)
    await i2c.register_i2c_device(var, config)

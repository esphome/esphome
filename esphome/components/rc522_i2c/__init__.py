import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, rc522
from esphome.const import CONF_ID

CODEOWNERS = ["@glmnet"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["rc522"]
MULTI_CONF = True

rc522_i2c_ns = cg.esphome_ns.namespace("rc522_i2c")
RC522I2C = rc522_i2c_ns.class_("RC522I2C", rc522.RC522, i2c.I2CDevice)

CONFIG_SCHEMA = cv.All(
    rc522.RC522_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(RC522I2C),
        }
    ).extend(i2c.i2c_device_schema(0x2C))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await rc522.setup_rc522(var, config)
    await i2c.register_i2c_device(var, config)

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, pn7150
from esphome.const import CONF_ID

AUTO_LOAD = ["pn7150"]
CODEOWNERS = ["@kbx81", "@jesserockz"]
DEPENDENCIES = ["i2c"]

pn7150_i2c_ns = cg.esphome_ns.namespace("pn7150_i2c")
PN7150I2C = pn7150_i2c_ns.class_("PN7150I2C", pn7150.PN7150, i2c.I2CDevice)

CONFIG_SCHEMA = cv.All(
    pn7150.PN7150_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(PN7150I2C),
        }
    ).extend(i2c.i2c_device_schema(0x28))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await pn7150.setup_pn7150(var, config)
    await i2c.register_i2c_device(var, config)

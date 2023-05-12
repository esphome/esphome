import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, pn7160
from esphome.const import CONF_ID

AUTO_LOAD = ["pn7160"]
CODEOWNERS = ["@kbx81", "@jesserockz"]
DEPENDENCIES = ["i2c"]

pn7160_i2c_ns = cg.esphome_ns.namespace("pn7160_i2c")
PN7160I2C = pn7160_i2c_ns.class_("PN7160I2C", pn7160.PN7160, i2c.I2CDevice)

CONFIG_SCHEMA = cv.All(
    pn7160.PN7160_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(PN7160I2C),
        }
    ).extend(i2c.i2c_device_schema(0x28))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await pn7160.setup_pn7160(var, config)
    await i2c.register_i2c_device(var, config)

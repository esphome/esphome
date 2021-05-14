import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, pn532
from esphome.const import CONF_ID

AUTO_LOAD = ["pn532"]
CODEOWNERS = ["@OttoWinter", "@jesserockz"]
DEPENDENCIES = ["i2c"]

pn532_i2c_ns = cg.esphome_ns.namespace("pn532_i2c")
PN532I2C = pn532_i2c_ns.class_("PN532I2C", pn532.PN532, i2c.I2CDevice)

CONFIG_SCHEMA = cv.All(
    pn532.PN532_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(PN532I2C),
        }
    ).extend(i2c.i2c_device_schema(0x24))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield pn532.setup_pn532(var, config)
    yield i2c.register_i2c_device(var, config)

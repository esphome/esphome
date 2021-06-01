import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ssd1327_base, i2c
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_PAGES

CODEOWNERS = ["@kbx81"]

AUTO_LOAD = ["ssd1327_base"]
DEPENDENCIES = ["i2c"]

ssd1327_i2c = cg.esphome_ns.namespace("ssd1327_i2c")
I2CSSD1327 = ssd1327_i2c.class_("I2CSSD1327", ssd1327_base.SSD1327, i2c.I2CDevice)

CONFIG_SCHEMA = cv.All(
    ssd1327_base.SSD1327_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(I2CSSD1327),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x3D)),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ssd1327_base.setup_ssd1327(var, config)
    await i2c.register_i2c_device(var, config)

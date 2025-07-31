import esphome.codegen as cg
from esphome.components import i2c, ssd1306_base
from esphome.components.ssd1306_base import _validate
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_PAGES

AUTO_LOAD = ["ssd1306_base"]
DEPENDENCIES = ["i2c"]

ssd1306_i2c = cg.esphome_ns.namespace("ssd1306_i2c")
I2CSSD1306 = ssd1306_i2c.class_("I2CSSD1306", ssd1306_base.SSD1306, i2c.I2CDevice)

CONFIG_SCHEMA = cv.All(
    ssd1306_base.SSD1306_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(I2CSSD1306),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x3C)),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ssd1306_base.setup_ssd1306(var, config)
    await i2c.register_i2c_device(var, config)

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@MrEditor97"]
AUTO_LOAD = ["adafruit_seesaw"]

af4991_ns = cg.esphome_ns.namespace("af4991")
AF4991 = af4991_ns.class_("AF4991", cg.Component, i2c.I2CDevice)

CONF_AF4991_ID = "af4991_id"

MULTI_CONF = True
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AF4991),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x36))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

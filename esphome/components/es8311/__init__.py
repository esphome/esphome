import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@kroimon"]

es8311_ns = cg.esphome_ns.namespace("es8311")
ES8311Component = es8311_ns.class_("ES8311Component", cg.Component, i2c.I2CDevice)

CONF_USE_MCLK = "use_mclk"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ES8311Component),
            cv.Optional(CONF_USE_MCLK): cv.boolean,
        }
    )
    .extend(i2c.i2c_device_schema(0x18))
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_USE_MCLK in config:
        cg.add(var.set_use_mclk(config[CONF_USE_MCLK]))

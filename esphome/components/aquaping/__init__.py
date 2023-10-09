import esphome.codegen as cg
from esphome.components import i2c
from esphome.const import CONF_ID
import esphome.config_validation as cv

CODEOWNERS = ["@sjtrny"]
DEPENDENCIES = ["i2c"]

aquaping_ns = cg.esphome_ns.namespace("aquaping")
AQUAPINGComponent = aquaping_ns.class_(
    "AQUAPINGComponent", cg.PollingComponent, i2c.I2CDevice
)

CONF_AQUAPING_ID = "aquaping_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AQUAPINGComponent),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x77))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

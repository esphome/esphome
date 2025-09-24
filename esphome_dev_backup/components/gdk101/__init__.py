import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@Szewcson"]

DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_GDK101_ID = "gdk101_id"

gdk101_ns = cg.esphome_ns.namespace("gdk101")
GDK101Component = gdk101_ns.class_(
    "GDK101Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GDK101Component),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x18))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

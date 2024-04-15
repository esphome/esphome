import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_ID

DEPENDENCIES = ["spi"]
CODEOWNERS = ["@clydebarrow"]

MULTI_CONF = True
spi_device_ns = cg.esphome_ns.namespace("spi_device")

spi_device = spi_device_ns.class_("SPIDeviceComponent", cg.Component, spi.SPIDevice)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(spi_device),
    }
).extend(spi.spi_device_schema(False, "1MHz", default_mode="MODE0"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

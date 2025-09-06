import esphome.codegen as cg
from esphome.components import ade7953_base, spi
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["ade7953_base"]

ade7953_ns = cg.esphome_ns.namespace("ade7953_spi")
ADE7953 = ade7953_ns.class_("AdE7953Spi", ade7953_base.ADE7953, spi.SPIDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADE7953),
        }
    )
    .extend(ade7953_base.ADE7953_CONFIG_SCHEMA)
    .extend(spi.spi_device_schema())
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await spi.register_spi_device(var, config)
    await ade7953_base.register_ade7953(var, config)

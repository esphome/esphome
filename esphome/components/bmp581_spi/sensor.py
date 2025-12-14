import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv

from ..bmp581_base import CONFIG_SCHEMA_BASE, to_code_base

AUTO_LOAD = ["bmp581_base"]
CODEOWNERS = ["@ademuri", "@danielkent-net"]
DEPENDENCIES = ["spi"]

bmp581_ns = cg.esphome_ns.namespace("bmp581_spi")
BMP581SPIComponent = bmp581_ns.class_(
    "BMP581SPIComponent", cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    spi.spi_device_schema(default_mode="mode3")
).extend({cv.GenerateID(): cv.declare_id(BMP581SPIComponent)})


async def to_code(config):
    var = await to_code_base(config)
    await spi.register_spi_device(var, config)

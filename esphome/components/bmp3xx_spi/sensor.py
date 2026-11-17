import esphome.codegen as cg
from esphome.components import spi
from esphome.types import ConfigType

from ..bmp3xx_base import bmp3xx_schema, to_code_base

AUTO_LOAD = ["bmp3xx_base"]
CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["spi"]

bmp3xx_ns = cg.esphome_ns.namespace("bmp3xx_spi")

BMP3XXSPIComponent = bmp3xx_ns.class_(
    "BMP3XXSPIComponent", cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = bmp3xx_schema(spi.spi_device_schema(), BMP3XXSPIComponent)


async def to_code(config: ConfigType) -> None:
    var = await to_code_base(config)
    await spi.register_spi_device(var, config)

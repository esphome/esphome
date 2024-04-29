import esphome.codegen as cg
from esphome.components import spi
from ..bmp3xx_base import to_code_base, cv, CONFIG_SCHEMA_BASE

AUTO_LOAD = ["bmp3xx_base"]
CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["spi"]

bmp3xx_ns = cg.esphome_ns.namespace("bmp3xx_spi")

BMP3XXSPIComponent = bmp3xx_ns.class_(
    "BMP3XXSPIComponent", cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(spi.spi_device_schema()).extend(
    {cv.GenerateID(): cv.declare_id(BMP3XXSPIComponent)}
)


async def to_code(config):
    var = await to_code_base(config)
    await spi.register_spi_device(var, config)

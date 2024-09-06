import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from ..bmp280_base import to_code_base, CONFIG_SCHEMA_BASE

AUTO_LOAD = ["bmp280_base"]
CODEOWNERS = ["@ademuri"]
DEPENDENCIES = ["spi"]

bmp280_ns = cg.esphome_ns.namespace("bmp280_spi")
BMP280SPIComponent = bmp280_ns.class_(
    "BMP280SPIComponent", cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    spi.spi_device_schema(default_mode="mode3")
).extend({cv.GenerateID(): cv.declare_id(BMP280SPIComponent)})


async def to_code(config):
    var = await to_code_base(config)
    await spi.register_spi_device(var, config)

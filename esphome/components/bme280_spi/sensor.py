import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from ..bme280_base import to_code_base, CONFIG_SCHEMA_BASE

AUTO_LOAD = ["bme280_base"]
CODEOWNERS = ["@apbodrov"]
DEPENDENCIES = ["spi"]


bme280_spi_ns = cg.esphome_ns.namespace("bme280_spi")
BME280SPIComponent = bme280_spi_ns.class_(
    "BME280SPIComponent", cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(spi.spi_device_schema()).extend(
    {cv.GenerateID(): cv.declare_id(BME280SPIComponent)}
)


async def to_code(config):
    var = await to_code_base(config)
    await spi.register_spi_device(var, config)

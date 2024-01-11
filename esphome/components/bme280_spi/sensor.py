import esphome.codegen as cg
from esphome.components import spi
from esphome.components.bme280_base.sensor import (
    to_code as to_code_base,
    cv,
    CONFIG_SCHEMA_BASE,
)

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["bme280_base"]


bme280_spi_ns = cg.esphome_ns.namespace("bme280_spi")
BME280SPIComponent = bme280_spi_ns.class_(
    "BME280SPIComponent", cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(spi.spi_device_schema()).extend(
    {cv.GenerateID(): cv.declare_id(BME280SPIComponent)}
)


async def to_code(config):
    await to_code_base(config, func=spi.register_spi_device)

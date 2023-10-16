import esphome.codegen as cg
from esphome.components import spi
from esphome.components.bme280_base.sensor import (
    to_code as to_code_base,
    bme280_ns,
    CONFIG_SCHEMA_BASE,
)

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["bme280_base"]

BME280SPIComponent = bme280_ns.class_(
    "BME280SPIComponent", cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(spi.spi_device_schema())

FUNC = spi.register_spi_device


async def to_code(config):
    await to_code_base(config)

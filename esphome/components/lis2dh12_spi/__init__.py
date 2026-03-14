import esphome.codegen as cg
from esphome.components import spi
from esphome.components.lis2dh12_base import (
    CONFIG_SCHEMA_BASE,
    LIS2DH12Component,
    to_code_base,
)
import esphome.config_validation as cv

CODEOWNERS = ["@latonita"]
AUTO_LOAD = ["lis2dh12_base"]
DEPENDENCIES = ["spi"]

MULTI_CONF = True

lis2dh12_spi_ns = cg.esphome_ns.namespace("lis2dh12_spi")
LIS2DH12SPIComponent = lis2dh12_spi_ns.class_(
    "LIS2DH12SPIComponent", LIS2DH12Component, spi.SPIDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    {
        cv.GenerateID(): cv.declare_id(LIS2DH12SPIComponent),
    }
).extend(spi.spi_device_schema(cs_pin_required=True))


async def to_code(config):
    var = await to_code_base(config)
    await spi.register_spi_device(var, config)

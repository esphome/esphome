import esphome.codegen as cg
from esphome.components import spi
from esphome.components.lis2dw12 import (
    CONFIG_SCHEMA_BASE,
    LIS2DW12Component,
    to_code_base,
    validate_data_rate,
)
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@latonita"]
AUTO_LOAD = ["lis2dw12"]
DEPENDENCIES = ["spi"]
DOMAIN = "lis2dw12_spi"
MULTI_CONF = True

lis2dw12_spi_ns = cg.esphome_ns.namespace("lis2dw12_spi")
LIS2DW12SPIComponent = lis2dw12_spi_ns.class_(
    "LIS2DW12SPIComponent",
    LIS2DW12Component,
    spi.SPIDevice,
)

CONFIG_SCHEMA = cv.All(
    CONFIG_SCHEMA_BASE.extend(
        {
            cv.GenerateID(): cv.declare_id(LIS2DW12SPIComponent),
        }
    ).extend(spi.spi_device_schema(cs_pin_required=True)),
    validate_data_rate,
)


async def to_code(config: ConfigType) -> None:
    var = await to_code_base(config)
    await spi.register_spi_device(var, config)

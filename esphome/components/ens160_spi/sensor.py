import esphome.codegen as cg
from esphome.components import spi
from esphome.components.ens160_base.sensor import (
    to_code as to_code_base,
    cv,
    CONFIG_SCHEMA_BASE,
)

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["ens160_base"]


ens160_spi_ns = cg.esphome_ns.namespace("ens160_spi")
ENS160SPIComponent = ens160_spi_ns.class_(
    "ENS160SPIComponent", cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(spi.spi_device_schema()).extend(
    {cv.GenerateID(): cv.declare_id(ENS160SPIComponent)}
)


async def to_code(config):
    await to_code_base(config, func=spi.register_spi_device)

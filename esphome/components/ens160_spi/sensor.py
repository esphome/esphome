import esphome.codegen as cg
from esphome.components import spi
from ..ens160_base import to_code_base, cv, CONFIG_SCHEMA_BASE

AUTO_LOAD = ["ens160_base"]
CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["spi"]

ens160_spi_ns = cg.esphome_ns.namespace("ens160_spi")

ENS160SPIComponent = ens160_spi_ns.class_(
    "ENS160SPIComponent", cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(spi.spi_device_schema()).extend(
    {cv.GenerateID(): cv.declare_id(ENS160SPIComponent)}
)


async def to_code(config):
    var = await to_code_base(config)
    await spi.register_spi_device(var, config)

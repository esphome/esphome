import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv

from ..spa06_base import CONFIG_SCHEMA_BASE, to_code_base

AUTO_LOAD = ["spa06_base"]
CODEOWNERS = ["@danielkent-net"]
DEPENDENCIES = ["spi"]

spa06_ns = cg.esphome_ns.namespace("spa06_spi")
SPA06SPIComponent = spa06_ns.class_(
    "SPA06SPIComponent", cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    spi.spi_device_schema(default_mode="mode3")
).extend({cv.GenerateID(): cv.declare_id(SPA06SPIComponent)})


async def to_code(config):
    var = await to_code_base(config)
    await spi.register_spi_device(var, config)

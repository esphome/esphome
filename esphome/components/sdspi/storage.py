import esphome.codegen as cg
from esphome.components import spi, storage
import esphome.config_validation as cv

CODEOWNERS = ["@abel-msk"]
# AUTO_LOAD = ["fatfs_esp32"]
DEPENDENCIES = ["spi"]

sdspi_ns = cg.esphome_ns.namespace("sdspi")
Sdspi = sdspi_ns.class_("SDSPI", storage.Storage, cg.PollingComponent)

CONFIG_SCHEMA = (
    storage.storage_schema(Sdspi)
    .extend(spi.spi_device_schema(cs_pin_required=True, default_mode="MODE0"))
    .extend(cv.polling_component_schema("10s"))
)


async def to_code(config):
    var = await storage.new_storage(config)
    await spi.register_spi_device(var, config)
    await cg.register_component(var, config)

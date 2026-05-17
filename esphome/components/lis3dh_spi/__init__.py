import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import CONF_ID

from ..lis3dh import CONFIG_SCHEMA_BASE, LIS3DHComponent, setup_lis3dh_base

AUTO_LOAD = ["lis3dh"]
CODEOWNERS = ["@jthoward64"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True

lis3dh_spi_ns = cg.esphome_ns.namespace("lis3dh_spi")
LIS3DHSPI = lis3dh_spi_ns.class_("LIS3DHSPI", LIS3DHComponent, spi.SPIDevice)

CONFIG_SCHEMA = (
    CONFIG_SCHEMA_BASE.extend({cv.GenerateID(): cv.declare_id(LIS3DHSPI)})
).extend(spi.spi_device_schema(cs_pin_required=True))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    await setup_lis3dh_base(var, config)

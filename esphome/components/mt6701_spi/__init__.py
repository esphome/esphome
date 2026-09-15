import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import CONF_ID

from ..mt6701 import MT6701Component

CODEOWNERS = ["@slimcdk"]
AUTO_LOAD = ["mt6701"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True

mt6701_spi_ns = cg.esphome_ns.namespace("mt6701_spi")
MT6701SPIComponent = mt6701_spi_ns.class_(
    "MT6701SPIComponent", MT6701Component, spi.SPIDevice
)

CONF_MT6701_SPI_ID = "mt6701_spi_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MT6701SPIComponent),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(
        spi.spi_device_schema(
            cs_pin_required=True,
            default_data_rate=1_000_000,
            default_mode="MODE1",
        )
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.types import ConfigType

from ..mt6701 import MT6701Component

CODEOWNERS = ["@slimcdk"]
AUTO_LOAD = ["mt6701"]
DEPENDENCIES = ["spi"]
DOMAIN = "mt6701_spi"
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
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        spi.spi_device_schema(
            cs_pin_required=True,
            default_data_rate=1_000_000,
            default_mode="MODE1",
        )
    )
)

# SSI only reads, so the bus needs its input line
FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "mt6701_spi", require_mosi=False, require_miso=True
)


async def to_code(config: ConfigType) -> None:
    await spi.new_spi_component(config)

import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@solomondg1"]
DEPENDENCIES = ["spi"]
DOMAIN = "ads1118"
MULTI_CONF = True

CONF_ADS1118_ID = "ads1118_id"

ads1118_ns = cg.esphome_ns.namespace("ads1118")
ADS1118 = ads1118_ns.class_("ADS1118", cg.Component, spi.SPIDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADS1118),
        }
    )
    .extend(spi.spi_device_schema(cs_pin_required=True))
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    await spi.new_spi_component(config)

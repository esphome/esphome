import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC, ICON_MAGNET
from esphome.types import ConfigType

from .. import CONF_MT6701_SPI_ID, MT6701SPIComponent, mt6701_spi_ns

DEPENDENCIES = ["mt6701_spi"]

MT6701SPIFieldStatus = mt6701_spi_ns.class_(
    "MT6701SPIFieldStatus",
    text_sensor.TextSensor,
    cg.PollingComponent,
    cg.Parented.template(MT6701SPIComponent),
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(
        MT6701SPIFieldStatus,
        icon=ICON_MAGNET,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )
    .extend(
        {
            cv.GenerateID(CONF_MT6701_SPI_ID): cv.use_id(MT6701SPIComponent),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config: ConfigType) -> None:
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_MT6701_SPI_ID])

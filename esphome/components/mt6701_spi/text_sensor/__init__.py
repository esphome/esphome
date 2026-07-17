import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC, ICON_MAGNET

from .. import CONF_MT6701_SPI_ID, MT6701SPIComponent

DEPENDENCIES = ["mt6701_spi"]

CONFIG_SCHEMA = text_sensor.text_sensor_schema(
    icon=ICON_MAGNET,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(
    {
        cv.GenerateID(CONF_MT6701_SPI_ID): cv.use_id(MT6701SPIComponent),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MT6701_SPI_ID])
    var = await text_sensor.new_text_sensor(config)
    cg.add(hub.set_field_status_text_sensor(var))

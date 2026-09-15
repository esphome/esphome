import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_MT6701_SPI_ID, MT6701SPIComponent

DEPENDENCIES = ["mt6701_spi"]

CONF_PUSH_BUTTON = "push_button"
CONF_TRACK_LOSS = "track_loss"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_MT6701_SPI_ID): cv.use_id(MT6701SPIComponent),
    cv.Optional(CONF_PUSH_BUTTON): binary_sensor.binary_sensor_schema(),
    cv.Optional(CONF_TRACK_LOSS): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MT6701_SPI_ID])
    if push_button_config := config.get(CONF_PUSH_BUTTON):
        sens = await binary_sensor.new_binary_sensor(push_button_config)
        cg.add(hub.set_push_button_binary_sensor(sens))
    if track_loss_config := config.get(CONF_TRACK_LOSS):
        sens = await binary_sensor.new_binary_sensor(track_loss_config)
        cg.add(hub.set_track_loss_binary_sensor(sens))

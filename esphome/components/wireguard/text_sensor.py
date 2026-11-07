import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_WIREGUARD_ID, Wireguard

DEPENDENCIES = ["wireguard"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_WIREGUARD_ID): cv.use_id(Wireguard),
    cv.Optional(CONF_ADDRESS): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_WIREGUARD_ID])

    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_ADDRESS, hub.set_address_sensor)

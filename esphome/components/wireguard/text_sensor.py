import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ADDRESS,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from . import Wireguard

CONF_WIREGUARD_ID = "wireguard_id"

DEPENDENCIES = ["wireguard"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_WIREGUARD_ID): cv.use_id(Wireguard),
    cv.Optional(CONF_ADDRESS): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_WIREGUARD_ID])

    if address_config := config.get(CONF_ADDRESS):
        sens = await text_sensor.new_text_sensor(address_config)
        cg.add(parent.set_address_sensor(sens))

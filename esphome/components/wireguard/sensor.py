import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_TIMESTAMP, ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_WIREGUARD_ID, Wireguard

CONF_LATEST_HANDSHAKE = "latest_handshake"

DEPENDENCIES = ["wireguard"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_WIREGUARD_ID): cv.use_id(Wireguard),
    cv.Optional(CONF_LATEST_HANDSHAKE): sensor.sensor_schema(
        device_class=DEVICE_CLASS_TIMESTAMP,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_WIREGUARD_ID])

    if latest_handshake_config := config.get(CONF_LATEST_HANDSHAKE):
        sens = await sensor.new_sensor(latest_handshake_config)
        cg.add(parent.set_handshake_sensor(sens))

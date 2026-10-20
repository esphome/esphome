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
    hub = await cg.get_variable(config[CONF_WIREGUARD_ID])

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_LATEST_HANDSHAKE, hub.set_handshake_sensor)

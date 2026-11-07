import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.const import CONF_ENABLED
import esphome.config_validation as cv
from esphome.const import (
    CONF_STATUS,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from . import CONF_WIREGUARD_ID, Wireguard

DEPENDENCIES = ["wireguard"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_WIREGUARD_ID): cv.use_id(Wireguard),
    cv.Optional(CONF_STATUS): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_CONNECTIVITY,
    ),
    cv.Optional(CONF_ENABLED): binary_sensor.binary_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_WIREGUARD_ID])

    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_STATUS, hub.set_status_sensor)
    await binary_sensors(CONF_ENABLED, hub.set_enabled_sensor)

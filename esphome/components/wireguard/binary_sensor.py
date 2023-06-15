import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_STATUS,
    DEVICE_CLASS_CONNECTIVITY,
)

from . import Wireguard

CONF_WIREGUARD_ID = "wireguard_id"

DEPENDENCIES = ["wireguard"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_WIREGUARD_ID): cv.use_id(Wireguard),
    cv.Optional(CONF_STATUS): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_CONNECTIVITY,
    ),
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_WIREGUARD_ID])

    if CONF_STATUS:
        sens = await binary_sensor.new_binary_sensor(config[CONF_STATUS])
        cg.add(parent.set_status_sensor(sens))

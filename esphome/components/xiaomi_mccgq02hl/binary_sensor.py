import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_LIGHT,
    CONF_TIMEOUT,
    DEVICE_CLASS_LIGHT,
    DEVICE_CLASS_OPENING,
)
from esphome.core import TimePeriod

from . import XiaomiMCCGQ02HL

DEPENDENCIES = ["xiaomi_mccgq02hl"]

CONF_OPEN = "open"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(XiaomiMCCGQ02HL),
        cv.Optional(CONF_LIGHT): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_LIGHT
        ),
        cv.Optional(CONF_OPEN): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_OPENING
        )
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if CONF_LIGHT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_LIGHT])
        cg.add(parent.set_light(sens))

    if CONF_OPEN in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_OPEN])
        cg.add(parent.set_open(sens))

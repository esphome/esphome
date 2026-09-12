import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_OCCUPANCY,
    DEVICE_CLASS_UPDATE,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from . import CONF_DFROBOT_C4001_ID, HUB_CHILD_SCHEMA

DEPENDENCIES = ["dfrobot_c4001"]


CONF_OCCUPANCY = "occupancy"
CONF_CONFIG_CHANGED = "config_changed"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Optional(CONF_OCCUPANCY): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_OCCUPANCY,
            ),
            cv.Optional(CONF_CONFIG_CHANGED): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_UPDATE,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(HUB_CHILD_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    sens0609_hub = await cg.get_variable(config[CONF_DFROBOT_C4001_ID])
    if occupancy := config.get(CONF_OCCUPANCY):
        bs = await binary_sensor.new_binary_sensor(occupancy)
        cg.add(sens0609_hub.set_occupancy_binary_sensor(bs))
    if config_changed := config.get(
        CONF_CONFIG_CHANGED,
    ):
        bs = await binary_sensor.new_binary_sensor(config_changed)
        cg.add(sens0609_hub.set_config_changed_binary_sensor(bs))

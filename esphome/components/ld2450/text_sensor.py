import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIRECTION,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_VERSION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ENTITY_CATEGORY_NONE,
    ICON_BLUETOOTH,
    ICON_CHIP,
    ICON_SIGN_DIRECTION,
)
from esphome.types import ConfigType

from . import CONF_LD2450_ID, LD2450Component

DEPENDENCIES = ["ld2450"]

MAX_TARGETS = 3

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
        cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon=ICON_CHIP,
        ),
        cv.Optional(CONF_MAC_ADDRESS): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon=ICON_BLUETOOTH,
        ),
    }
)

CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
    {
        cv.Optional(f"target_{n + 1}"): cv.Schema(
            {
                cv.Optional(CONF_DIRECTION): text_sensor.text_sensor_schema(
                    entity_category=ENTITY_CATEGORY_NONE,
                    icon=ICON_SIGN_DIRECTION,
                ),
            }
        )
        for n in range(MAX_TARGETS)
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2450_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_VERSION, hub.set_version_text_sensor)
    await text_sensors(CONF_MAC_ADDRESS, hub.set_mac_text_sensor)
    for n in range(MAX_TARGETS):
        if (direction_conf := config.get(f"target_{n + 1}")) and (
            direction_config := direction_conf.get(CONF_DIRECTION)
        ):
            sens = await text_sensor.new_text_sensor(direction_config)
            cg.add(hub.set_direction_text_sensor(n, sens))

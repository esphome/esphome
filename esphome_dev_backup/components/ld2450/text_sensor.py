import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIRECTION,
    CONF_MAC_ADDRESS,
    CONF_VERSION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ENTITY_CATEGORY_NONE,
    ICON_BLUETOOTH,
    ICON_CHIP,
    ICON_SIGN_DIRECTION,
)

from . import CONF_LD2450_ID, LD2450Component

DEPENDENCIES = ["ld2450"]

MAX_TARGETS = 3

CONFIG_SCHEMA = cv.Schema(
    {
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


async def to_code(config):
    ld2450_component = await cg.get_variable(config[CONF_LD2450_ID])
    if version_config := config.get(CONF_VERSION):
        sens = await text_sensor.new_text_sensor(version_config)
        cg.add(ld2450_component.set_version_text_sensor(sens))
    if mac_address_config := config.get(CONF_MAC_ADDRESS):
        sens = await text_sensor.new_text_sensor(mac_address_config)
        cg.add(ld2450_component.set_mac_text_sensor(sens))
    for n in range(MAX_TARGETS):
        if direction_conf := config.get(f"target_{n + 1}"):
            if direction_config := direction_conf.get(CONF_DIRECTION):
                sens = await text_sensor.new_text_sensor(direction_config)
                cg.add(ld2450_component.set_direction_text_sensor(n, sens))

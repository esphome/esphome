import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE,
    CONF_POWER_SAVE_MODE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_CHIP,
    ICON_RESTART,
    ICON_WIFI,
)

from . import CONF_DEBUG_ID, DebugComponent

DEPENDENCIES = ["debug"]


CONF_RESET_REASON = "reset_reason"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DEBUG_ID): cv.use_id(DebugComponent),
        cv.Optional(CONF_DEVICE): text_sensor.text_sensor_schema(
            icon=ICON_CHIP,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_RESET_REASON): text_sensor.text_sensor_schema(
            icon=ICON_RESTART,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_POWER_SAVE_MODE): cv.All(
            text_sensor.text_sensor_schema(
                icon=ICON_WIFI,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.only_on(["esp32"]),
            cv.requires_component("wifi"),
        ),
    }
)


async def to_code(config):
    debug_component = await cg.get_variable(config[CONF_DEBUG_ID])

    if CONF_DEVICE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DEVICE])
        cg.add(debug_component.set_device_info_sensor(sens))
    if CONF_RESET_REASON in config:
        sens = await text_sensor.new_text_sensor(config[CONF_RESET_REASON])
        cg.add(debug_component.set_reset_reason_sensor(sens))
    if CONF_POWER_SAVE_MODE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_POWER_SAVE_MODE])
        cg.add(debug_component.set_wifi_power_save_sensor(sens))

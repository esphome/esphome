import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_CHIP,
    ICON_RESTART,
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

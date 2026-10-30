import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_VERSION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_BLUETOOTH,
    ICON_CHIP,
)
from esphome.types import ConfigType

from . import CONF_LD2410_ID, LD2410Component, require_background_correction_button

DEPENDENCIES = ["ld2410"]

CONF_CALIBRATION_RESULT = "calibration_result"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
    ),
    cv.Optional(CONF_MAC_ADDRESS): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_BLUETOOTH
    ),
    cv.Optional(CONF_CALIBRATION_RESULT): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:radar"
    ),
}

FINAL_VALIDATE_SCHEMA = require_background_correction_button(CONF_CALIBRATION_RESULT)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2410_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_VERSION, hub.set_version_text_sensor)
    await text_sensors(CONF_MAC_ADDRESS, hub.set_mac_text_sensor)
    await text_sensors(CONF_CALIBRATION_RESULT, hub.set_calibration_result_text_sensor)

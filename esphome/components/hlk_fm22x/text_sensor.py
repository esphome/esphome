import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.const import CONF_SERIAL_NUMBER
import esphome.config_validation as cv
from esphome.const import (
    CONF_VERSION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_ACCOUNT,
    ICON_CHIP,
    ICON_RESTART,
)
from esphome.types import ConfigType

from . import CONF_HLK_FM22X_ID, ICON_FACE_RECOGNITION, HlkFm22xComponent

DEPENDENCIES = ["hlk_fm22x"]

CONF_LAST_FACE_NAME = "last_face_name"
CONF_FACE_STATE = "face_state"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLK_FM22X_ID): cv.use_id(HlkFm22xComponent),
        cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
            icon=ICON_RESTART,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(
            icon=ICON_CHIP,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_LAST_FACE_NAME): text_sensor.text_sensor_schema(
            icon=ICON_ACCOUNT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_FACE_STATE): text_sensor.text_sensor_schema(
            icon=ICON_FACE_RECOGNITION,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_HLK_FM22X_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_VERSION, hub.set_version_text_sensor)
    await text_sensors(CONF_SERIAL_NUMBER, hub.set_serial_number_text_sensor)
    await text_sensors(CONF_LAST_FACE_NAME, hub.set_last_face_name_text_sensor)
    await text_sensors(CONF_FACE_STATE, hub.set_face_state_text_sensor)

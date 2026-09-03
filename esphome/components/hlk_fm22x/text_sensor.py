import esphome.codegen as cg
from esphome.components import text_sensor
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
CONF_SERIAL_NUMBER = "serial_number"
CONF_FACE_STATE = "face_state"

SENSOR_KEYS = (CONF_VERSION, CONF_SERIAL_NUMBER, CONF_LAST_FACE_NAME, CONF_FACE_STATE)

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
    for key in SENSOR_KEYS:
        if (conf := config.get(key)) is not None:
            sens = await text_sensor.new_text_sensor(conf)
            cg.add(getattr(hub, f"set_{key}_text_sensor")(sens))

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_VERSION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_ACCOUNT,
    ICON_RESTART,
)

from . import CONF_HLK_FM22X_ID, HlkFm22xComponent

DEPENDENCIES = ["hlk_fm22x"]

CONF_LAST_FACE_NAME = "last_face_name"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLK_FM22X_ID): cv.use_id(HlkFm22xComponent),
        cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
            icon=ICON_RESTART,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_LAST_FACE_NAME): text_sensor.text_sensor_schema(
            icon=ICON_ACCOUNT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HLK_FM22X_ID])
    for key in [
        CONF_VERSION,
        CONF_LAST_FACE_NAME,
    ]:
        if key not in config:
            continue
        conf = config[key]
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_text_sensor")(sens))

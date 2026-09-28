import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_STATUS, ENTITY_CATEGORY_DIAGNOSTIC, ICON_ACCOUNT

from . import CONF_HLK_FM22X_ID, HlkFm22xComponent

DEPENDENCIES = ["hlk_fm22x"]

CONF_FACE_COUNT = "face_count"
CONF_LAST_FACE_ID = "last_face_id"
ICON_FACE = "mdi:face-recognition"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLK_FM22X_ID): cv.use_id(HlkFm22xComponent),
        cv.Optional(CONF_FACE_COUNT): sensor.sensor_schema(
            icon=ICON_FACE,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_STATUS): sensor.sensor_schema(
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_LAST_FACE_ID): sensor.sensor_schema(
            icon=ICON_ACCOUNT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HLK_FM22X_ID])

    for key in [
        CONF_FACE_COUNT,
        CONF_STATUS,
        CONF_LAST_FACE_ID,
    ]:
        if key not in config:
            continue
        conf = config[key]
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))

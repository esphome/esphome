import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_STATUS, ENTITY_CATEGORY_DIAGNOSTIC, ICON_ACCOUNT
from esphome.types import ConfigType

from . import CONF_HLK_FM22X_ID, ICON_FACE_RECOGNITION, HlkFm22xComponent

DEPENDENCIES = ["hlk_fm22x"]

CONF_FACE_COUNT = "face_count"
CONF_LAST_FACE_ID = "last_face_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLK_FM22X_ID): cv.use_id(HlkFm22xComponent),
        cv.Optional(CONF_FACE_COUNT): sensor.sensor_schema(
            icon=ICON_FACE_RECOGNITION,
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


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_HLK_FM22X_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_FACE_COUNT, hub.set_face_count_sensor)
    await sensors(CONF_STATUS, hub.set_status_sensor)
    await sensors(CONF_LAST_FACE_ID, hub.set_last_face_id_sensor)

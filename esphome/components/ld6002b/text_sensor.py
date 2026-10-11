import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import LD6002BComponent
from .const import CONF_LD6002B_ID, CONF_OTA_VERSION, CONF_WORK_MODE

DEPENDENCIES = ["ld6002b"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD6002B_ID): cv.use_id(LD6002BComponent),
        cv.Optional(CONF_WORK_MODE): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_OTA_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD6002B_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_WORK_MODE, hub.set_work_mode_text_sensor)
    await text_sensors(CONF_OTA_VERSION, hub.set_ota_version_text_sensor)

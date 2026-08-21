import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

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


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LD6002B_ID])
    if work_mode_config := config.get(CONF_WORK_MODE):
        sens = await text_sensor.new_text_sensor(work_mode_config)
        cg.add(hub.set_work_mode_text_sensor(sens))
    if ota_config := config.get(CONF_OTA_VERSION):
        sens = await text_sensor.new_text_sensor(ota_config)
        cg.add(hub.set_ota_version_text_sensor(sens))

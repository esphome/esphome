import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_VERSION, ENTITY_CATEGORY_DIAGNOSTIC, ICON_CHIP

from . import CONF_TFLUNA_ID, TFLunaComponent

DEPENDENCIES = ["tfluna"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_TFLUNA_ID): cv.use_id(TFLunaComponent),
    cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
    ),
}


async def to_code(config):
    tfluna_component = await cg.get_variable(config[CONF_TFLUNA_ID])
    if version_config := config.get(CONF_VERSION):
        sens = await text_sensor.new_text_sensor(version_config)
        cg.add(tfluna_component.set_version_text_sensor(sens))

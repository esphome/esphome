import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_VERSION, ENTITY_CATEGORY_DIAGNOSTIC, ICON_CHIP
from esphome.types import ConfigType

from . import CONF_LD2410S_ID, LD2410SComponent

DEPENDENCIES = ["ld2410s"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410S_ID): cv.use_id(LD2410SComponent),
    cv.Required(CONF_VERSION): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
    ),
}


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_LD2410S_VERSION")
    hub = await cg.get_variable(config[CONF_LD2410S_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_VERSION, hub.set_version_text_sensor)

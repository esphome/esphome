import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_VERSION, ENTITY_CATEGORY_DIAGNOSTIC, ICON_CHIP
from esphome.types import ConfigType

from . import CONF_DFROBOT_SEN0609_ID, DFRobotSEN0609Component

DEPENDENCIES = ["dfrobot_sen0609"]

CONF_HARDWARE_VERSION = "hardware_version"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_DFROBOT_SEN0609_ID): cv.use_id(DFRobotSEN0609Component),
    cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
    ),
    cv.Optional(CONF_HARDWARE_VERSION): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_SEN0609_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_VERSION, hub.set_version_text_sensor)
    await text_sensors(CONF_HARDWARE_VERSION, hub.set_hardware_version_text_sensor)

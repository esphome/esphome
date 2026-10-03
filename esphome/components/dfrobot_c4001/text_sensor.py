import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

# from esphome import core
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC, ICON_CHIP

from . import CONF_DFROBOT_C4001_ID, HUB_CHILD_SCHEMA

CONF_SOFTWARE_VERSION = "software_version"
CONF_HARDWARE_VERSION = "hardware_version"

DEPENDENCIES = ["dfrobot_c4001"]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Optional(CONF_SOFTWARE_VERSION): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
            ),
            cv.Optional(CONF_HARDWARE_VERSION): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
            ),
        }
    )
    .extend(HUB_CHILD_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    dfrobot_c4001_hub = await cg.get_variable(config[CONF_DFROBOT_C4001_ID])

    if software_version := config.get(CONF_SOFTWARE_VERSION):
        sens = await text_sensor.new_text_sensor(software_version)
        cg.add(dfrobot_c4001_hub.set_software_version_text_sensor(sens))
    if hardware_version := config.get(CONF_HARDWARE_VERSION):
        sens = await text_sensor.new_text_sensor(hardware_version)
        cg.add(dfrobot_c4001_hub.set_hardware_version_text_sensor(sens))

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_CHIP,
    ICON_FINGERPRINT,
)
from esphome.types import ConfigType

from . import CONF_UFM01_ID, UFM01Component

DEPENDENCIES = ["ufm01"]

CONF_SOFTWARE_VERSION = "software_version"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_UFM01_ID): cv.use_id(UFM01Component),
    cv.Optional(CONF_DEVICE_ID): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_FINGERPRINT
    ),
    cv.Optional(CONF_SOFTWARE_VERSION): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
    ),
}


async def to_code(config: ConfigType) -> None:
    ufm01_component = await cg.get_variable(config[CONF_UFM01_ID])

    if device_id_config := config.get(CONF_DEVICE_ID):
        sens = await text_sensor.new_text_sensor(device_id_config)
        cg.add(ufm01_component.set_device_id_text_sensor(sens))

    if software_version_config := config.get(CONF_SOFTWARE_VERSION):
        sens = await text_sensor.new_text_sensor(software_version_config)
        cg.add(ufm01_component.set_software_version_text_sensor(sens))

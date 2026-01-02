import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAC_ADDRESS,
    CONF_VERSION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_BLUETOOTH,
    ICON_CHIP,
)

from . import CONF_LD2412_ID, LD2412Component

DEPENDENCIES = ["ld2412"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2412_ID): cv.use_id(LD2412Component),
    cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
    ),
    cv.Optional(CONF_MAC_ADDRESS): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_BLUETOOTH
    ),
}


async def to_code(config):
    LD2412_component = await cg.get_variable(config[CONF_LD2412_ID])
    if version_config := config.get(CONF_VERSION):
        sens = await text_sensor.new_text_sensor(version_config)
        cg.add(LD2412_component.set_version_text_sensor(sens))
    if mac_address_config := config.get(CONF_MAC_ADDRESS):
        sens = await text_sensor.new_text_sensor(mac_address_config)
        cg.add(LD2412_component.set_mac_text_sensor(sens))

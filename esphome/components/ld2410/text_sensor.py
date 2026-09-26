import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_VERSION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_BLUETOOTH,
    ICON_CHIP,
)
from esphome.types import ConfigType

from . import CONF_LD2410_ID, LD2410Component

DEPENDENCIES = ["ld2410"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
    ),
    cv.Optional(CONF_MAC_ADDRESS): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_BLUETOOTH
    ),
}


async def to_code(config: ConfigType) -> None:
    ld2410_component = await cg.get_variable(config[CONF_LD2410_ID])
    await text_sensor.new_sub_text_sensor(
        config, CONF_VERSION, ld2410_component.set_version_text_sensor
    )
    await text_sensor.new_sub_text_sensor(
        config, CONF_MAC_ADDRESS, ld2410_component.set_mac_text_sensor
    )

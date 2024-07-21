import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    ENTITY_CATEGORY_NONE,
)
from ..climate import (
    CONF_HAIER_ID,
    HonClimate,
)

CODEOWNERS = ["@paveldn"]
TextSensorTypeEnum = HonClimate.enum("SubTextSensorType", True)

# Haier text sensors
CONF_CLEANING_STATUS = "cleaning_status"
CONF_PROTOCOL_VERSION = "protocol_version"
CONF_APPLIANCE_NAME = "appliance_name"

# Additional icons
ICON_SPRAY_BOTTLE = "mdi:spray-bottle"
ICON_TEXT_BOX = "mdi:text-box-outline"

TEXT_SENSOR_TYPES = {
    CONF_CLEANING_STATUS: text_sensor.text_sensor_schema(
        icon=ICON_SPRAY_BOTTLE,
        entity_category=ENTITY_CATEGORY_NONE,
    ),
    CONF_PROTOCOL_VERSION: text_sensor.text_sensor_schema(
        icon=ICON_TEXT_BOX,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_APPLIANCE_NAME: text_sensor.text_sensor_schema(
        icon=ICON_TEXT_BOX,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HAIER_ID): cv.use_id(HonClimate),
    }
).extend({cv.Optional(type): schema for type, schema in TEXT_SENSOR_TYPES.items()})


async def to_code(config):
    paren = await cg.get_variable(config[CONF_HAIER_ID])

    for type_ in TEXT_SENSOR_TYPES:
        if conf := config.get(type_):
            sens = await text_sensor.new_text_sensor(conf)
            text_sensor_type = getattr(TextSensorTypeEnum, type_.upper())
            cg.add(paren.set_sub_text_sensor(text_sensor_type, sens))

import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.const import CONF_LIBRETINY
import esphome.config_validation as cv
from esphome.const import (
    CONF_VERSION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_CELLPHONE_ARROW_DOWN,
)

from .const import LTComponent

DEPENDENCIES = ["libretiny"]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LIBRETINY): cv.use_id(LTComponent),
        cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
            icon=ICON_CELLPHONE_ARROW_DOWN,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LIBRETINY])

    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_VERSION, hub.set_version_sensor)

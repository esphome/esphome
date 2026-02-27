import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_VERSION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_CELLPHONE_ARROW_DOWN,
)

from .const import CONF_LIBRETINY, LTComponent

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
    lt_component = await cg.get_variable(config[CONF_LIBRETINY])

    if CONF_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_VERSION])
        cg.add(lt_component.set_version_sensor(sens))

from esphome.components import text_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_CHIP,
    CONF_ADDRESS,
    CONF_DALLAS_ID,
)

from . import DallasComponent

DEPENDENCIES = ["dallas"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DALLAS_ID): cv.use_id(DallasComponent),
        cv.Optional(CONF_ADDRESS): text_sensor.text_sensor_schema(
            icon=ICON_CHIP,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DALLAS_ID])

    if CONF_ADDRESS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_ADDRESS])
        cg.add(hub.set_address_sensor(sens))

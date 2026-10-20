import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_VERSION, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import CONF_ECOCOMFORT2_ID, ECOCOMFORT2_CLIENT_SCHEMA

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

CONFIG_SCHEMA = ECOCOMFORT2_CLIENT_SCHEMA.extend(
    {
        cv.Required(CONF_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ECOCOMFORT2_ID])
    sensors = text_sensor.sub_text_sensors(config)
    await sensors(CONF_VERSION, hub.set_firmware_text_sensor)

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_LOG, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import CONF_TSDB_ID, TsdbComponent

DEPENDENCIES = ["tsdb"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TSDB_ID): cv.use_id(TsdbComponent),
        cv.Optional(CONF_LOG): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_TSDB_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_LOG, hub.set_log_sensor)

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC, STATE_CLASS_TOTAL_INCREASING
from esphome.types import ConfigType

from . import CONF_TSDB_ID, TsdbComponent

DEPENDENCIES = ["tsdb"]

CONF_ERRORS = "errors"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TSDB_ID): cv.use_id(TsdbComponent),
        cv.Optional(CONF_ERRORS): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_TSDB_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_ERRORS, hub.set_errors_sensor)

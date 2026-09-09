import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from .. import Alpha3
from ..const import CONF_ACTIVE_CONTROL_SOURCE, CONF_ALPHA3_ID, CONF_REALIZED_OPERATION

DEPENDENCIES = ["alpha3"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ALPHA3_ID): cv.use_id(Alpha3),
        cv.Optional(CONF_REALIZED_OPERATION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_ACTIVE_CONTROL_SOURCE): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ALPHA3_ID])
    if realized_operation_config := config.get(CONF_REALIZED_OPERATION):
        entity = await text_sensor.new_text_sensor(realized_operation_config)
        cg.add(hub.set_realized_operation_text_sensor(entity))
    if active_control_source_config := config.get(CONF_ACTIVE_CONTROL_SOURCE):
        entity = await text_sensor.new_text_sensor(active_control_source_config)
        cg.add(hub.set_active_control_source_text_sensor(entity))

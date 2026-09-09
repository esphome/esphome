import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from .. import Alpha3
from ..const import CONF_ALPHA3_ID, CONF_READY

DEPENDENCIES = ["alpha3"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ALPHA3_ID): cv.use_id(Alpha3),
        cv.Optional(CONF_READY): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ALPHA3_ID])
    if ready_config := config.get(CONF_READY):
        entity = await binary_sensor.new_binary_sensor(ready_config)
        cg.add(hub.set_ready_binary_sensor(entity))

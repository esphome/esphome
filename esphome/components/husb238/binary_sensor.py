import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_NAME, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import CONF_HUSB238_ID, Husb238Component

DEPENDENCIES = ["husb238"]

CONF_ATTACHED = "attached"
CONF_CC_DIRECTION = "cc_direction"

_DIAGNOSTIC_SCHEMA = cv.maybe_simple_value(
    binary_sensor.binary_sensor_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    key=CONF_NAME,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HUSB238_ID): cv.use_id(Husb238Component),
        cv.Optional(CONF_ATTACHED): _DIAGNOSTIC_SCHEMA,
        cv.Optional(CONF_CC_DIRECTION): _DIAGNOSTIC_SCHEMA,
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_HUSB238_ID])
    sensors = binary_sensor.sub_binary_sensors(config)
    await sensors(CONF_ATTACHED, hub.set_attached_binary_sensor)
    await sensors(CONF_CC_DIRECTION, hub.set_cc_direction_binary_sensor)

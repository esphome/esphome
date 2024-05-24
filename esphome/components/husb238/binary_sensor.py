import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_NAME, ENTITY_CATEGORY_DIAGNOSTIC
from . import Husb238Component, CONF_HUSB238_ID

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["husb238"]

CONF_ATTACHED = "attached"
CONF_CC_DIRECTION = "cc_direction"

TYPES = [CONF_ATTACHED]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_HUSB238_ID): cv.use_id(Husb238Component),
            cv.Optional(CONF_ATTACHED): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_CC_DIRECTION): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                key=CONF_NAME,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        var = await binary_sensor.new_binary_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_binary_sensor")(var))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HUSB238_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)

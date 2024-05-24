import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_NAME, CONF_STATUS, ENTITY_CATEGORY_DIAGNOSTIC
from . import Husb238Component, CONF_HUSB238_ID

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["husb238"]

CONF_CAPABILITIES = "capabilities"

TYPES = [CONF_STATUS, CONF_CAPABILITIES]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_HUSB238_ID): cv.use_id(Husb238Component),
            cv.Optional(CONF_STATUS): cv.maybe_simple_value(
                text_sensor.text_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_CAPABILITIES): cv.maybe_simple_value(
                text_sensor.text_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                key=CONF_NAME,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        var = await text_sensor.new_text_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_text_sensor")(var))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HUSB238_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)

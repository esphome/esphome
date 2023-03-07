import esphome.codegen as cg
from esphome.components import text_sensor

from .. import (
    HOME_ASSISTANT_IMPORT_SCHEMA,
    homeassistant_ns,
    setup_home_assistant_entity,
)

DEPENDENCIES = ["api"]

HomeassistantTextSensor = homeassistant_ns.class_(
    "HomeassistantTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(HomeassistantTextSensor).extend(
    HOME_ASSISTANT_IMPORT_SCHEMA
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    setup_home_assistant_entity(var, config)

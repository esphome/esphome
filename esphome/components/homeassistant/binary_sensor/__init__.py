import esphome.codegen as cg
from esphome.components import binary_sensor

from .. import (
    HOME_ASSISTANT_IMPORT_SCHEMA,
    homeassistant_ns,
    setup_home_assistant_entity,
)

DEPENDENCIES = ["api"]

HomeassistantBinarySensor = homeassistant_ns.class_(
    "HomeassistantBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(HomeassistantBinarySensor).extend(
    HOME_ASSISTANT_IMPORT_SCHEMA
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    setup_home_assistant_entity(var, config)

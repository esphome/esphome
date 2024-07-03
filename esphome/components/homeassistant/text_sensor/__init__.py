import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.automation import ENTITY_STATE_TRIGGER_SCHEMA, setup_entity_state_trigger

from .. import (
    HOME_ASSISTANT_IMPORT_SCHEMA,
    homeassistant_ns,
    setup_home_assistant_entity,
)

DEPENDENCIES = ["api"]

HomeassistantTextSensor = homeassistant_ns.class_(
    "HomeassistantTextSensor", text_sensor.TextSensor, cg.Component, cg.EntityBase_State
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(HomeassistantTextSensor)
    .extend(HOME_ASSISTANT_IMPORT_SCHEMA)
    .extend(ENTITY_STATE_TRIGGER_SCHEMA)
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    await setup_entity_state_trigger(var, config)
    setup_home_assistant_entity(var, config)

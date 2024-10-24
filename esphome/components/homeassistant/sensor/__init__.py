import esphome.codegen as cg
from esphome.components import sensor
from esphome.automation import ENTITY_STATE_TRIGGER_SCHEMA, setup_entity_state_trigger

from .. import (
    HOME_ASSISTANT_IMPORT_SCHEMA,
    homeassistant_ns,
    setup_home_assistant_entity,
)

DEPENDENCIES = ["api"]

HomeassistantSensor = homeassistant_ns.class_(
    "HomeassistantSensor", sensor.Sensor, cg.Component, cg.EntityBase_State
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(HomeassistantSensor, accuracy_decimals=1)
    .extend(HOME_ASSISTANT_IMPORT_SCHEMA)
    .extend(ENTITY_STATE_TRIGGER_SCHEMA)
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await setup_entity_state_trigger(var, config)
    setup_home_assistant_entity(var, config)

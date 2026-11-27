from esphome.components import text_sensor
from esphome.types import ConfigType

from .. import (
    HOME_ASSISTANT_IMPORT_SCHEMA,
    final_validate_entity_id,
    homeassistant_ns,
    register_home_assistant_import,
    setup_home_assistant_entity,
)

DEPENDENCIES = ["api"]

HomeassistantTextSensor = homeassistant_ns.class_(
    "HomeassistantTextSensor", text_sensor.TextSensor
)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(HomeassistantTextSensor).extend(
    HOME_ASSISTANT_IMPORT_SCHEMA
)

FINAL_VALIDATE_SCHEMA = final_validate_entity_id


async def to_code(config: ConfigType) -> None:
    var = await text_sensor.new_text_sensor(config)
    setup_home_assistant_entity(var, config)
    await register_home_assistant_import(var)

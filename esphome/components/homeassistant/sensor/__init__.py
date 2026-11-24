from esphome.components import sensor
from esphome.types import ConfigType

from .. import (
    HOME_ASSISTANT_IMPORT_SCHEMA,
    final_validate_entity_id,
    homeassistant_ns,
    register_home_assistant_import,
    setup_home_assistant_entity,
)

DEPENDENCIES = ["api"]
# For is_unset() in homeassistant.h
AUTO_LOAD = ["homeassistant"]

HomeassistantSensor = homeassistant_ns.class_("HomeassistantSensor", sensor.Sensor)

CONFIG_SCHEMA = sensor.sensor_schema(HomeassistantSensor, accuracy_decimals=1).extend(
    HOME_ASSISTANT_IMPORT_SCHEMA
)

FINAL_VALIDATE_SCHEMA = final_validate_entity_id


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    setup_home_assistant_entity(var, config)
    await register_home_assistant_import(var)

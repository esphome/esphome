from esphome.components import binary_sensor
from esphome.types import ConfigType

from . import register_remote_binary_sensor, remote_entity_schema

CONFIG_SCHEMA = remote_entity_schema(binary_sensor.binary_sensor_schema())


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    await register_remote_binary_sensor(config, var)

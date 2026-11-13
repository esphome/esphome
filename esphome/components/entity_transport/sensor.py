from esphome.components import sensor
from esphome.types import ConfigType

from . import register_remote_sensor, remote_entity_schema

CONFIG_SCHEMA = remote_entity_schema(sensor.sensor_schema())


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await register_remote_sensor(config, var)

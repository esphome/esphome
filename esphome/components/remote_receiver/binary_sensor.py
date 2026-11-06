from esphome.components import binary_sensor, remote_base
from esphome.types import ConfigType

DEPENDENCIES = ["remote_receiver"]

CONFIG_SCHEMA = remote_base.validate_binary_sensor


async def to_code(config: ConfigType) -> None:
    var = await remote_base.build_binary_sensor(config)
    await binary_sensor.register_binary_sensor(var, config)

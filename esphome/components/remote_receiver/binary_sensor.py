from esphome.components import binary_sensor, remote_base
from esphome.types import ConfigType

from . import FILTER_SOURCE_FILES  # noqa: F401  pylint: disable=unused-import

DEPENDENCIES = ["remote_receiver"]

CONFIG_SCHEMA = remote_base.validate_binary_sensor


async def to_code(config: ConfigType) -> None:
    var = await remote_base.build_binary_sensor(config)
    await binary_sensor.register_binary_sensor(var, config)

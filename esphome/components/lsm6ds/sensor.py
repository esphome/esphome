from esphome.components.motion import (
    temperature_sensor_schema,
    temperature_sensor_to_code,
)
from esphome.types import ConfigType

from . import CONF_LSM6DS_ID, LSM6DSComponent

CONFIG_SCHEMA = temperature_sensor_schema(CONF_LSM6DS_ID, LSM6DSComponent)


async def to_code(config: ConfigType) -> None:
    await temperature_sensor_to_code(config, CONF_LSM6DS_ID)

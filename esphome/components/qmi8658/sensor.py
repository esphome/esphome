from esphome.components.motion import (
    temperature_sensor_schema,
    temperature_sensor_to_code,
)
from esphome.types import ConfigType

from . import CONF_QMI8658_ID, QMI8658Component

CONFIG_SCHEMA = temperature_sensor_schema(CONF_QMI8658_ID, QMI8658Component)


async def to_code(config: ConfigType) -> None:
    await temperature_sensor_to_code(config, CONF_QMI8658_ID)

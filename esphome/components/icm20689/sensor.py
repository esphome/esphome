from esphome.components.motion import (
    temperature_sensor_schema,
    temperature_sensor_to_code,
)
from esphome.types import ConfigType

from . import CONF_ICM20689_ID, ICM20689Component

AUTO_LOAD = ["icm20689"]

CONFIG_SCHEMA = temperature_sensor_schema(CONF_ICM20689_ID, ICM20689Component)


async def to_code(config: ConfigType) -> None:
    await temperature_sensor_to_code(config, CONF_ICM20689_ID)

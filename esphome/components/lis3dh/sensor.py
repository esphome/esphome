from esphome.components.motion import (
    temperature_sensor_schema,
    temperature_sensor_to_code,
)
from esphome.types import ConfigType

from . import CONF_LIS3DH_ID, LIS3DHComponent

# The LIS3DH temperature sensor is uncalibrated and only reports relative
# changes (roughly 1 °C per count), so a single decimal of accuracy is plenty.
CONFIG_SCHEMA = temperature_sensor_schema(
    CONF_LIS3DH_ID, LIS3DHComponent, accuracy_decimals=1
)


async def to_code(config: ConfigType) -> None:
    await temperature_sensor_to_code(config, CONF_LIS3DH_ID)

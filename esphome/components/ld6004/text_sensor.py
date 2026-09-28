from esphome.components.ld600x import entities
from esphome.types import ConfigType

from . import LD6004Component
from .const import CONF_LD6004_ID

DEPENDENCIES = ["ld6004"]

CONFIG_SCHEMA = entities.text_sensor_schema(LD6004Component, CONF_LD6004_ID)


async def to_code(config: ConfigType) -> None:
    await entities.text_sensor_to_code(config, CONF_LD6004_ID)

from esphome.components.ld600x import entities
from esphome.components.ld600x.const import (
    CONF_LOW_POWER,
    CONF_POINT_CLOUD,
    CONF_TARGET_DISPLAY,
)
from esphome.types import ConfigType

from .. import LD6002BComponent
from ..const import CONF_LD6002B_ID

KEYS = (CONF_LOW_POWER, CONF_POINT_CLOUD, CONF_TARGET_DISPLAY)

DEPENDENCIES = ["ld6002b"]

CONFIG_SCHEMA = entities.switch_schema(LD6002BComponent, CONF_LD6002B_ID, keys=KEYS)


async def to_code(config: ConfigType) -> None:
    await entities.switch_to_code(config, CONF_LD6002B_ID, keys=KEYS)

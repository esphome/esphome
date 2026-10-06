from esphome.components.ld600x import entities
from esphome.components.ld600x.const import CONF_POINT_CLOUD, CONF_TARGET_DISPLAY
from esphome.types import ConfigType

from .. import LD6004Component
from ..const import CONF_LD6004_ID

KEYS = (CONF_POINT_CLOUD, CONF_TARGET_DISPLAY)

DEPENDENCIES = ["ld6004"]

CONFIG_SCHEMA = entities.switch_schema(LD6004Component, CONF_LD6004_ID, keys=KEYS)


async def to_code(config: ConfigType) -> None:
    await entities.switch_to_code(config, CONF_LD6004_ID, keys=KEYS)

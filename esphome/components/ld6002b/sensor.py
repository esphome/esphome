from esphome.components.ld600x import entities
from esphome.types import ConfigType

from . import LD6002BComponent
from .const import AREA_KINDS, CONF_LD6002B_ID

DEPENDENCIES = ["ld6002b"]

CONFIG_SCHEMA = entities.sensor_schema(
    LD6002BComponent, CONF_LD6002B_ID, area_kinds=AREA_KINDS
)


async def to_code(config: ConfigType) -> None:
    await entities.sensor_to_code(config, CONF_LD6002B_ID, area_kinds=AREA_KINDS)

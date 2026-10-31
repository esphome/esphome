from esphome.components.ld600x import entities
from esphome.types import ConfigType

from .. import LD6002BComponent
from ..const import CONF_LD6002B_ID

DEPENDENCIES = ["ld6002b"]

CONFIG_SCHEMA = entities.number_schema(LD6002BComponent, CONF_LD6002B_ID)
FINAL_VALIDATE_SCHEMA = entities.number_final_validate(CONF_LD6002B_ID, "ld6002b")


async def to_code(config: ConfigType) -> None:
    await entities.number_to_code(config, CONF_LD6002B_ID)

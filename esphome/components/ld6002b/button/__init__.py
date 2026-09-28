from esphome.components.ld600x import entities
from esphome.components.ld600x.const import CONF_WAKE
from esphome.types import ConfigType

from .. import LD6002BComponent
from ..const import CONF_LD6002B_ID

KEYS = tuple(entities.BUTTON_MAP)

DEPENDENCIES = ["ld6002b"]

CONFIG_SCHEMA = entities.button_schema(LD6002BComponent, CONF_LD6002B_ID, keys=KEYS)
FINAL_VALIDATE_SCHEMA = entities.button_final_validate(
    CONF_LD6002B_ID, "ld6002b", wake_key=CONF_WAKE
)


async def to_code(config: ConfigType) -> None:
    await entities.button_to_code(config, CONF_LD6002B_ID)

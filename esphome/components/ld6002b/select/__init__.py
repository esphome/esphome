from esphome.components import select
from esphome.components.ld600x import entities
from esphome.types import ConfigType

from .. import LD6002BComponent
from ..const import AREA_KINDS, CONF_LD6002B_ID

DEPENDENCIES = ["ld6002b"]

CONFIG_SCHEMA = entities.select_schema(LD6002BComponent, CONF_LD6002B_ID)
FINAL_VALIDATE_SCHEMA = entities.unique_settings_final_validate(
    select.DOMAIN, CONF_LD6002B_ID, "ld6002b"
)


async def to_code(config: ConfigType) -> None:
    await entities.select_to_code(config, CONF_LD6002B_ID, area_kinds=AREA_KINDS)

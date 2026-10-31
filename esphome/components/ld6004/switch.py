from esphome.components import switch
from esphome.components.ld600x import entities
from esphome.types import ConfigType

from . import LD6004Component
from .const import CONF_LD6004_ID

DEPENDENCIES = ["ld6004"]

CONFIG_SCHEMA = entities.switch_schema(LD6004Component, CONF_LD6004_ID)
FINAL_VALIDATE_SCHEMA = entities.unique_settings_final_validate(
    switch.DOMAIN, CONF_LD6004_ID, "ld6004"
)


async def to_code(config: ConfigType) -> None:
    await entities.switch_to_code(config, CONF_LD6004_ID)

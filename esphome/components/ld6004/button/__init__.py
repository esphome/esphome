from esphome.components import button
from esphome.components.ld600x import entities
from esphome.components.ld600x.const import CONF_WAKE
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import LD6004Component, ld6004_ns
from ..const import CONF_LD6004_ID

DEPENDENCIES = ["ld6004"]

CONF_CLEAR_DWELL = "clear_dwell"
KEYS = tuple(key for key in entities.BUTTON_MAP if key != CONF_WAKE)

LD6004ButtonType = ld6004_ns.enum("LD6004ButtonType")

CONFIG_SCHEMA = entities.button_schema(
    LD6004Component,
    CONF_LD6004_ID,
    keys=KEYS,
    extra={
        cv.Optional(CONF_CLEAR_DWELL): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_CONFIG
        ),
    },
)
FINAL_VALIDATE_SCHEMA = entities.button_final_validate(CONF_LD6004_ID, "ld6004")


async def to_code(config: ConfigType) -> None:
    await entities.button_to_code(
        config,
        CONF_LD6004_ID,
        extra={CONF_CLEAR_DWELL: LD6004ButtonType.BUTTON_CLEAR_DWELL},
    )

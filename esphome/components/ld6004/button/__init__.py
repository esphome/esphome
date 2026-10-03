import esphome.codegen as cg
from esphome.components import button
from esphome.components.ld600x import entities
from esphome.components.ld600x.const import CONF_WAKE
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from .. import LD6004Component, ld6004_ns
from ..const import CONF_LD6004_ID

DEPENDENCIES = ["ld6004"]

CONF_CLEAR_DWELL = "clear_dwell"
KEYS = tuple(key for key in entities.BUTTON_MAP if key != CONF_WAKE)

LD6004ButtonType = ld6004_ns.enum("LD6004ButtonType")

CONFIG_SCHEMA = entities.button_schema(
    LD6004Component, CONF_LD6004_ID, keys=KEYS
).extend(
    {
        cv.Optional(CONF_CLEAR_DWELL): button.button_schema(
            entities.LD600XButton, entity_category=ENTITY_CATEGORY_CONFIG
        ),
    }
)
FINAL_VALIDATE_SCHEMA = entities.button_final_validate(CONF_LD6004_ID, "ld6004")


async def to_code(config: ConfigType) -> None:
    await entities.button_to_code(config, CONF_LD6004_ID)
    if button_config := config.get(CONF_CLEAR_DWELL):
        var: MockObj = cg.new_Pvariable(
            button_config[CONF_ID], LD6004ButtonType.BUTTON_CLEAR_DWELL
        )
        await button.register_button(var, button_config)
        await cg.register_parented(var, config[CONF_LD6004_ID])

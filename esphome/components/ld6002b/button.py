from esphome.components.ld600x import entities
import esphome.config_validation as cv
from esphome.const import CONF_WAKEUP_PIN
from esphome.core import ID
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import LD6002BComponent, ld6002b_ns
from .const import CONF_LD6002B_ID, CONF_WAKE

DEPENDENCIES = ["ld6002b"]

LD6002BButtonType = ld6002b_ns.enum("LD6002BButtonType")

CONFIG_SCHEMA = entities.button_schema(
    LD6002BComponent,
    CONF_LD6002B_ID,
    extra={cv.Optional(CONF_WAKE): entities.DIAGNOSTIC_BUTTON_SCHEMA},
)
_shared_final_validate = entities.button_final_validate(CONF_LD6002B_ID, "ld6002b")


def _final_validate(config: ConfigType) -> None:
    _shared_final_validate(config)
    if not config.get(CONF_WAKE):
        return
    hub_id: ID = config[CONF_LD6002B_ID]
    hub_config = fv.get_declaring_config(hub_id, fv.full_config.get())
    if hub_config.get(CONF_WAKEUP_PIN) is None:
        raise cv.Invalid(
            f"{CONF_WAKE} requires {CONF_WAKEUP_PIN} on the parent ld6002b component",
            path=[CONF_WAKE],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    await entities.button_to_code(
        config,
        CONF_LD6002B_ID,
        extra={CONF_WAKE: LD6002BButtonType.BUTTON_WAKE},
    )

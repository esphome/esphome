import esphome.codegen as cg
from esphome.components import button, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_FACTORY_RESET,
    CONF_ID,
    CONF_RESTART,
    ENTITY_CATEGORY_CONFIG,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_LD2450_ID, LD2450Component
from .text_sensor import CONF_XY, MAX_TARGETS

CONF_XY_UNFILTERED = "xy_unfiltered"

ICON_FILTER_OFF = "mdi:filter-off"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
    cv.Optional(CONF_FACTORY_RESET): button.factory_reset_button_schema(
        button.ApplyButton
    ),
    cv.Optional(CONF_RESTART): button.restart_button_schema(button.ApplyButton),
    cv.Optional(CONF_XY_UNFILTERED): button.button_schema(
        button.ApplyButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_FILTER_OFF,
    ),
}


def _final_validate(config: ConfigType) -> None:
    """xy_unfiltered needs an xy text sensor on the same LD2450."""
    if CONF_XY_UNFILTERED not in config:
        return
    hub_id = config[CONF_LD2450_ID].id
    for conf in fv.platform_entries(domain="text_sensor", platform="ld2450"):
        if conf[CONF_LD2450_ID].id == hub_id and any(
            CONF_XY in conf.get(f"target_{n + 1}", {}) for n in range(MAX_TARGETS)
        ):
            return
    raise cv.Invalid(
        "xy_unfiltered needs an ld2450 text sensor with xy", path=[CONF_XY_UNFILTERED]
    )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2450_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(
        CONF_FACTORY_RESET, hub.set_factory_reset_button, apply="factory_reset()"
    )
    await buttons(
        CONF_RESTART, hub.set_restart_button, apply="restart_and_read_all_info()"
    )
    if CONF_XY_UNFILTERED in config:
        cg.add_define("USE_LD2450_XY_UNFILTERED")
        text_sensor.request_filter_reset()
    await buttons(
        CONF_XY_UNFILTERED, hub.set_xy_unfiltered_button, apply="start_xy_unfiltered()"
    )

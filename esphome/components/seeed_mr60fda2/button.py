import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_FACTORY_RESET,
    DEVICE_CLASS_RESTART,
    DEVICE_CLASS_UPDATE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ENTITY_CATEGORY_NONE,
)
from esphome.types import ConfigType

from . import CONF_MR60FDA2_ID, MR60FDA2Component

DEPENDENCIES = ["seeed_mr60fda2"]


CONF_GET_RADAR_PARAMETERS = "get_radar_parameters"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR60FDA2_ID): cv.use_id(MR60FDA2Component),
    cv.Optional(CONF_GET_RADAR_PARAMETERS): button.button_schema(
        button.ApplyButton,
        device_class=DEVICE_CLASS_UPDATE,
        entity_category=ENTITY_CATEGORY_NONE,
    ),
    cv.Optional(CONF_FACTORY_RESET): button.button_schema(
        button.ApplyButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MR60FDA2_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(
        CONF_GET_RADAR_PARAMETERS,
        hub.set_get_radar_parameters_button,
        apply="get_radar_parameters()",
    )
    await buttons(
        CONF_FACTORY_RESET, hub.set_factory_reset_button, apply="factory_reset()"
    )

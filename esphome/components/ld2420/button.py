import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_FACTORY_RESET,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_DATABASE,
    ICON_RESTART,
    ICON_RESTART_ALERT,
)
from esphome.types import ConfigType

from .. import CONF_LD2420_ID, LD2420Component

CONF_APPLY_CONFIG = "apply_config"
CONF_REVERT_CONFIG = "revert_config"
CONF_RESTART_MODULE = "restart_module"


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2420_ID): cv.use_id(LD2420Component),
    cv.Required(CONF_APPLY_CONFIG): button.button_schema(
        button.ApplyButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART_ALERT,
    ),
    cv.Optional(CONF_REVERT_CONFIG): button.button_schema(
        button.ApplyButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART,
    ),
    cv.Optional(CONF_RESTART_MODULE): button.button_schema(
        button.ApplyButton,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon=ICON_DATABASE,
    ),
    cv.Optional(CONF_FACTORY_RESET): button.button_schema(
        button.ApplyButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_DATABASE,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2420_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(
        CONF_APPLY_CONFIG, hub.set_apply_config_button, apply="apply_config_action()"
    )
    await buttons(
        CONF_REVERT_CONFIG,
        hub.set_revert_config_button,
        apply="revert_config_action()",
    )
    await buttons(
        CONF_RESTART_MODULE,
        hub.set_restart_module_button,
        apply="restart_module_action()",
    )
    await buttons(
        CONF_FACTORY_RESET,
        hub.set_factory_reset_button,
        apply="factory_reset_action()",
    )

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

from .. import CONF_LD2420_ID, LD2420Component, ld2420_ns

LD2420ApplyConfigButton = ld2420_ns.class_("LD2420ApplyConfigButton", button.Button)
LD2420RevertConfigButton = ld2420_ns.class_("LD2420RevertConfigButton", button.Button)
LD2420RestartModuleButton = ld2420_ns.class_("LD2420RestartModuleButton", button.Button)
LD2420FactoryResetButton = ld2420_ns.class_("LD2420FactoryResetButton", button.Button)

CONF_APPLY_CONFIG = "apply_config"
CONF_REVERT_CONFIG = "revert_config"
CONF_RESTART_MODULE = "restart_module"


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2420_ID): cv.use_id(LD2420Component),
    cv.Required(CONF_APPLY_CONFIG): button.button_schema(
        LD2420ApplyConfigButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART_ALERT,
    ),
    cv.Optional(CONF_REVERT_CONFIG): button.button_schema(
        LD2420RevertConfigButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART,
    ),
    cv.Optional(CONF_RESTART_MODULE): button.button_schema(
        LD2420RestartModuleButton,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon=ICON_DATABASE,
    ),
    cv.Optional(CONF_FACTORY_RESET): button.button_schema(
        LD2420FactoryResetButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_DATABASE,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2420_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(CONF_APPLY_CONFIG, hub.set_apply_config_button)
    await buttons(CONF_REVERT_CONFIG, hub.set_revert_config_button)
    await buttons(CONF_RESTART_MODULE, hub.set_restart_module_button)
    await buttons(CONF_FACTORY_RESET, hub.set_factory_reset_button)

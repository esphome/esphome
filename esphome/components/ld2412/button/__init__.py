import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_FACTORY_RESET,
    CONF_ID,
    CONF_RESTART,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_DATABASE,
    ICON_PULSE,
    ICON_RESTART,
    ICON_RESTART_ALERT,
)
from esphome.types import ConfigType

from .. import CONF_LD2412_ID, LD2412_ns, LD2412Component

FactoryResetButton = LD2412_ns.class_("FactoryResetButton", button.Button)
QueryButton = LD2412_ns.class_("QueryButton", button.Button)
RestartButton = LD2412_ns.class_("RestartButton", button.Button)
StartDynamicBackgroundCorrectionButton = LD2412_ns.class_(
    "StartDynamicBackgroundCorrectionButton", button.Button
)

CONF_QUERY_PARAMS = "query_params"
CONF_START_DYNAMIC_BACKGROUND_CORRECTION = "start_dynamic_background_correction"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2412_ID): cv.use_id(LD2412Component),
    cv.Optional(CONF_FACTORY_RESET): button.button_schema(
        FactoryResetButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART_ALERT,
    ),
    cv.Optional(CONF_QUERY_PARAMS): button.button_schema(
        QueryButton,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon=ICON_DATABASE,
    ),
    cv.Optional(CONF_RESTART): button.button_schema(
        RestartButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon=ICON_RESTART,
    ),
    cv.Optional(CONF_START_DYNAMIC_BACKGROUND_CORRECTION): button.button_schema(
        StartDynamicBackgroundCorrectionButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_PULSE,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2412_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(CONF_FACTORY_RESET, hub.set_factory_reset_button)
    await buttons(CONF_QUERY_PARAMS, hub.set_query_button)
    await buttons(CONF_RESTART, hub.set_restart_button)
    await buttons(
        CONF_START_DYNAMIC_BACKGROUND_CORRECTION,
        hub.set_start_dynamic_background_correction_button,
    )

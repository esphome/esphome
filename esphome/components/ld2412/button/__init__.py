import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_FACTORY_RESET,
    CONF_RESTART,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_DATABASE,
    ICON_PULSE,
    ICON_RESTART,
    ICON_RESTART_ALERT,
)

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


async def to_code(config):
    LD2412_component = await cg.get_variable(config[CONF_LD2412_ID])
    if factory_reset_config := config.get(CONF_FACTORY_RESET):
        b = await button.new_button(factory_reset_config)
        await cg.register_parented(b, config[CONF_LD2412_ID])
        cg.add(LD2412_component.set_factory_reset_button(b))
    if query_params_config := config.get(CONF_QUERY_PARAMS):
        b = await button.new_button(query_params_config)
        await cg.register_parented(b, config[CONF_LD2412_ID])
        cg.add(LD2412_component.set_query_button(b))
    if restart_config := config.get(CONF_RESTART):
        b = await button.new_button(restart_config)
        await cg.register_parented(b, config[CONF_LD2412_ID])
        cg.add(LD2412_component.set_restart_button(b))
    if start_dynamic_background_correction_config := config.get(
        CONF_START_DYNAMIC_BACKGROUND_CORRECTION
    ):
        b = await button.new_button(start_dynamic_background_correction_config)
        await cg.register_parented(b, config[CONF_LD2412_ID])
        cg.add(LD2412_component.set_start_dynamic_background_correction_button(b))

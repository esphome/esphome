import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_FACTORY_RESET,
    CONF_RESTART,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_RESTART,
    ICON_RESTART_ALERT,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_TFLUNA_ID, FACTORY_DEFAULT_ADDRESS, TFLunaComponent, tfluna_ns

DEPENDENCIES = ["tfluna"]

ResetButton = tfluna_ns.class_("ResetButton", button.Button)
RestartButton = tfluna_ns.class_("RestartButton", button.Button)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_TFLUNA_ID): cv.use_id(TFLunaComponent),
    cv.Optional(CONF_FACTORY_RESET): button.button_schema(
        ResetButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART_ALERT,
    ),
    cv.Optional(CONF_RESTART): button.button_schema(
        RestartButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon=ICON_RESTART,
    ),
}


def _validate_factory_default_address(config: ConfigType) -> ConfigType:
    if config.get(CONF_ADDRESS) != FACTORY_DEFAULT_ADDRESS:
        raise cv.Invalid(
            f"'{CONF_FACTORY_RESET}' requires the TF-Luna to use its factory default "
            f"I2C address 0x{FACTORY_DEFAULT_ADDRESS:02X}, as a factory reset "
            "restores that address",
            path=[CONF_ADDRESS],
        )
    return config


def _final_validate(config: ConfigType) -> ConfigType:
    if CONF_FACTORY_RESET in config:
        fv.id_declaration_match_schema(_validate_factory_default_address)(
            config[CONF_TFLUNA_ID]
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    tfluna_component = await cg.get_variable(config[CONF_TFLUNA_ID])
    if factory_reset_config := config.get(CONF_FACTORY_RESET):
        b = await button.new_button(factory_reset_config)
        await cg.register_parented(b, config[CONF_TFLUNA_ID])
        cg.add(tfluna_component.set_reset_button(b))
    if restart_config := config.get(CONF_RESTART):
        b = await button.new_button(restart_config)
        await cg.register_parented(b, config[CONF_TFLUNA_ID])
        cg.add(tfluna_component.set_restart_button(b))

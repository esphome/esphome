import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_FACTORY_RESET, CONF_RESTART
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_TFLUNA_ID, FACTORY_DEFAULT_ADDRESS, TFLunaComponent

DEPENDENCIES = ["tfluna"]


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_TFLUNA_ID): cv.use_id(TFLunaComponent),
    cv.Optional(CONF_FACTORY_RESET): button.factory_reset_button_schema(
        button.ApplyButton
    ),
    cv.Optional(CONF_RESTART): button.restart_button_schema(button.ApplyButton),
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
    hub = await cg.get_variable(config[CONF_TFLUNA_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(CONF_FACTORY_RESET, hub.set_reset_button, apply="factory_reset()")
    await buttons(CONF_RESTART, hub.set_restart_button, apply="restart()")

import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_ID,
    CONF_SENSITIVITY,
    ENTITY_CATEGORY_CONFIG,
)
from esphome.types import ConfigType

from .. import CONF_LD2460_ID, LD2460Component, ld2460_ns

CONF_INSTALLATION_MODE = "installation_mode"

ICON_ROTATE_LEFT = "mdi:rotate-left"
ICON_TUNE = "mdi:tune"

BaudRateSelect = ld2460_ns.class_("BaudRateSelect", select.Select)
InstallationModeSelect = ld2460_ns.class_("InstallationModeSelect", select.Select)
SensitivitySelect = ld2460_ns.class_("SensitivitySelect", select.Select)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2460_ID): cv.use_id(LD2460Component),
    cv.Optional(CONF_BAUD_RATE): select.select_schema(
        BaudRateSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
    cv.Optional(CONF_INSTALLATION_MODE): select.select_schema(
        InstallationModeSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_ROTATE_LEFT,
    ),
    cv.Optional(CONF_SENSITIVITY): select.select_schema(
        SensitivitySelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_TUNE,
    ),
}


async def to_code(config: ConfigType) -> None:
    ld2460_component = await cg.get_variable(config[CONF_LD2460_ID])
    if baud_rate_config := config.get(CONF_BAUD_RATE):
        s = await select.new_select(
            baud_rate_config,
            options=[
                "9600",
                "19200",
                "38400",
                "57600",
                "115200",
                "230400",
                "256000",
                "460800",
            ],
        )
        await cg.register_parented(s, config[CONF_LD2460_ID])
        cg.add(ld2460_component.set_baud_rate_select(s))
    if installation_mode_config := config.get(CONF_INSTALLATION_MODE):
        s = await select.new_select(
            installation_mode_config,
            options=[
                "Side",
                "Top",
            ],
        )
        await cg.register_parented(s, config[CONF_LD2460_ID])
        cg.add(ld2460_component.set_installation_mode_select(s))
    if sensitivity_config := config.get(CONF_SENSITIVITY):
        s = await select.new_select(
            sensitivity_config,
            options=[
                "High",
                "Medium",
                "Low",
            ],
        )
        await cg.register_parented(s, config[CONF_LD2460_ID])
        cg.add(ld2460_component.set_sensitivity_select(s))

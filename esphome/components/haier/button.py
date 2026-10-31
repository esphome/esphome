from esphome.components import button
import esphome.config_validation as cv
from esphome.types import ConfigType

from ..climate import CONF_HAIER_ID, HonClimate

CODEOWNERS = ["@paveldn"]


# Haier buttons
CONF_SELF_CLEANING = "self_cleaning"
CONF_STERI_CLEANING = "steri_cleaning"

# Additional icons
ICON_SPRAY_BOTTLE = "mdi:spray-bottle"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HAIER_ID): cv.use_id(HonClimate),
        cv.Optional(CONF_SELF_CLEANING): button.button_schema(
            button.ApplyButton,
            icon=ICON_SPRAY_BOTTLE,
        ),
        cv.Optional(CONF_STERI_CLEANING): button.button_schema(
            button.ApplyButton,
            icon=ICON_SPRAY_BOTTLE,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    buttons = button.sub_buttons(config, parent=config[CONF_HAIER_ID])
    await buttons(CONF_SELF_CLEANING, None, apply="start_self_cleaning()")
    await buttons(CONF_STERI_CLEANING, None, apply="start_steri_cleaning()")

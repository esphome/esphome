from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_LIGHT
from esphome.types import ConfigType

from .climate import HaierTundra

CODEOWNERS = ["@fauxpark"]

ICON_LED_OUTLINE = "mdi:led-outline"
ICON_SPRAY_BOTTLE = "mdi:spray-bottle"

CONF_HAIER_TUNDRA_ID = "haier_tundra_id"
CONF_SELF_CLEAN = "self_clean"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_HAIER_TUNDRA_ID): cv.use_id(HaierTundra),
        cv.Optional(CONF_LIGHT): button.button_schema(
            button.ApplyButton, icon=ICON_LED_OUTLINE
        ),
        cv.Optional(CONF_SELF_CLEAN): button.button_schema(
            button.ApplyButton, icon=ICON_SPRAY_BOTTLE
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    buttons = button.sub_buttons(config, parent=config[CONF_HAIER_TUNDRA_ID])
    await buttons(CONF_LIGHT, None, apply="toggle_light()")
    await buttons(CONF_SELF_CLEAN, None, apply="start_self_clean()")

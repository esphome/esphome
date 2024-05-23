import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from ..climate import (
    CONF_HAIER_ID,
    HonClimate,
    haier_ns,
)

CODEOWNERS = ["@paveldn"]
SelfCleaningButton = haier_ns.class_("SelfCleaningButton", button.Button)
SteriCleaningButton = haier_ns.class_("SteriCleaningButton", button.Button)


# Haier buttons
CONF_SELF_CLEANING = "self_cleaning"
CONF_STERI_CLEANING = "steri_cleaning"

# Additional icons
ICON_SPRAY_BOTTLE = "mdi:spray-bottle"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_HAIER_ID): cv.use_id(HonClimate),
        cv.Optional(CONF_SELF_CLEANING): button.button_schema(
            SelfCleaningButton,
            icon=ICON_SPRAY_BOTTLE,
        ),
        cv.Optional(CONF_STERI_CLEANING): button.button_schema(
            SteriCleaningButton,
            icon=ICON_SPRAY_BOTTLE,
        ),
    }
)


async def to_code(config):
    for button_type in [CONF_SELF_CLEANING, CONF_STERI_CLEANING]:
        if conf := config.get(button_type):
            btn = await button.new_button(conf)
            await cg.register_parented(btn, config[CONF_HAIER_ID])

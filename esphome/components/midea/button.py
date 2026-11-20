import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.types import ConfigType

from .climate import CONF_MIDEA_ID, AirConditioner

CODEOWNERS = ["@dudanov"]

CONF_SELF_CLEAN = "self_clean"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MIDEA_ID): cv.use_id(AirConditioner),
        # Starts the self clean (active clean) cycle
        cv.Optional(CONF_SELF_CLEAN): button.button_schema(
            button.ApplyButton, icon="mdi:broom"
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MIDEA_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(CONF_SELF_CLEAN, None, apply="do_self_clean()")

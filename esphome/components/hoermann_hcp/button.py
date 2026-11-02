from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ICON_AIR_FILTER
from esphome.types import ConfigType

from . import CONF_HOERMANN_HCP_ID, HoermannHcp

DEPENDENCIES = ["hoermann_hcp"]

CONF_HALF_OPEN = "half_open"
CONF_VENT = "vent"

ICON_GARAGE_OPEN_VARIANT = "mdi:garage-open-variant"


BUTTON_KEYS = (CONF_VENT, CONF_HALF_OPEN)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_HOERMANN_HCP_ID): cv.use_id(HoermannHcp),
            cv.Optional(CONF_VENT): button.button_schema(
                button.ApplyButton, icon=ICON_AIR_FILTER
            ),
            cv.Optional(CONF_HALF_OPEN): button.button_schema(
                button.ApplyButton, icon=ICON_GARAGE_OPEN_VARIANT
            ),
        }
    ),
    cv.has_at_least_one_key(*BUTTON_KEYS),
)


async def to_code(config: ConfigType) -> None:
    buttons = button.sub_buttons(config, parent=config[CONF_HOERMANN_HCP_ID])
    await buttons(CONF_VENT, None, apply="vent_door()")
    await buttons(CONF_HALF_OPEN, None, apply="half_open_door()")

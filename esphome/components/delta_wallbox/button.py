from esphome.components import button
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import CONF_DELTA_WALLBOX_ID, DeltaWallbox

DEPENDENCIES = ["delta_wallbox"]

CONF_START_CHARGING = "start_charging"
CONF_STOP_CHARGING = "stop_charging"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DELTA_WALLBOX_ID): cv.use_id(DeltaWallbox),
        cv.Optional(CONF_START_CHARGING): button.button_schema(
            button.ApplyButton,
            icon="mdi:play-circle-outline",
        ),
        cv.Optional(CONF_STOP_CHARGING): button.button_schema(
            button.ApplyButton,
            icon="mdi:stop-circle-outline",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    buttons = button.sub_buttons(config, parent=config[CONF_DELTA_WALLBOX_ID])
    await buttons(CONF_START_CHARGING, None, apply="start_charging()")
    await buttons(CONF_STOP_CHARGING, None, apply="stop_charging()")

from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_SAFE_MODE,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART_ALERT,
)
from esphome.types import ConfigType

from . import SafeModeComponent

DEPENDENCIES = ["safe_mode"]

CONFIG_SCHEMA = button.button_schema(
    button.ApplyButton,
    device_class=DEVICE_CLASS_RESTART,
    entity_category=ENTITY_CATEGORY_CONFIG,
    icon=ICON_RESTART_ALERT,
).extend({cv.GenerateID(CONF_SAFE_MODE): cv.use_id(SafeModeComponent)})


async def to_code(config: ConfigType) -> None:
    buttons = button.sub_buttons(config, parent=config[CONF_SAFE_MODE])
    await buttons.create(config, None, apply="restart_in_safe_mode()")

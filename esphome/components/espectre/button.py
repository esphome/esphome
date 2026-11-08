from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from . import CONF_ESPECTRE_ID, ESPectreComponent

DEPENDENCIES = ["espectre"]


CONFIG_SCHEMA = button.button_schema(
    button.ApplyButton, entity_category=ENTITY_CATEGORY_CONFIG
).extend({cv.GenerateID(CONF_ESPECTRE_ID): cv.use_id(ESPectreComponent)})


async def to_code(config: ConfigType) -> None:
    buttons = button.sub_buttons(config, parent=config[CONF_ESPECTRE_ID])
    await buttons.create(config, None, apply="recalibrate()")

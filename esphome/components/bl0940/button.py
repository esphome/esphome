from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_RESTART
from esphome.types import ConfigType

from . import CONF_BL0940_ID
from .sensor import BL0940

CONFIG_SCHEMA = button.button_schema(
    button.ApplyButton,
    entity_category=ENTITY_CATEGORY_CONFIG,
    icon=ICON_RESTART,
).extend({cv.GenerateID(CONF_BL0940_ID): cv.use_id(BL0940)})


async def to_code(config: ConfigType) -> None:
    await button.sub_buttons(config, parent=config[CONF_BL0940_ID]).create(
        config, None, apply="reset_calibration()"
    )

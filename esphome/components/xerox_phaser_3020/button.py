from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import CONF_XEROX_PHASER_3020_ID, XeroxPhaser3020

DEPENDENCIES = ["xerox_phaser_3020"]

CONF_QUERY_STATUS = "query_status"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_XEROX_PHASER_3020_ID): cv.use_id(XeroxPhaser3020),
        # Asks the printer for its status with PJL and logs the answer
        cv.Optional(CONF_QUERY_STATUS): button.button_schema(
            button.ApplyButton,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:printer-search",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    buttons = button.sub_buttons(config, parent=config[CONF_XEROX_PHASER_3020_ID])
    await buttons(CONF_QUERY_STATUS, None, apply="query_status()")

from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_SOURCE_ID,
)
from esphome.core.entity_helpers import inherit_property_from
from esphome.types import ConfigType

CONFIG_SCHEMA = button.button_schema(button.ApplyButton).extend(
    {
        cv.Required(CONF_SOURCE_ID): cv.use_id(button.Button),
    }
)

FINAL_VALIDATE_SCHEMA = cv.All(
    inherit_property_from(CONF_ICON, CONF_SOURCE_ID),
    inherit_property_from(CONF_ENTITY_CATEGORY, CONF_SOURCE_ID),
    inherit_property_from(CONF_DEVICE_CLASS, CONF_SOURCE_ID),
)


async def to_code(config: ConfigType) -> None:
    await button.sub_buttons(config, parent=config[CONF_SOURCE_ID]).create(
        config, None, apply="press()"
    )

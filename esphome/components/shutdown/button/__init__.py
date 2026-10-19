from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_POWER
from esphome.types import ConfigType

CONFIG_SCHEMA = button.button_schema(
    button.ApplyButton, entity_category=ENTITY_CATEGORY_CONFIG, icon=ICON_POWER
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    await button.sub_buttons(config).create(
        config, None, apply="shutdown::shutdown_device()"
    )

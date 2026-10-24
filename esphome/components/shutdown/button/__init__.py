from esphome.components import button
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_POWER
from esphome.types import ConfigType

CONFIG_SCHEMA = button.button_schema(
    button.ApplyButton, entity_category=ENTITY_CATEGORY_CONFIG, icon=ICON_POWER
)


async def to_code(config: ConfigType) -> None:
    await button.sub_buttons(config).create(
        config, None, apply="shutdown::shutdown_device()"
    )

from esphome.components import button
from esphome.const import (
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART_ALERT,
)
from esphome.types import ConfigType

CONFIG_SCHEMA = button.button_schema(
    button.ApplyButton,
    device_class=DEVICE_CLASS_RESTART,
    entity_category=ENTITY_CATEGORY_CONFIG,
    icon=ICON_RESTART_ALERT,
)


async def to_code(config: ConfigType) -> None:
    await button.sub_buttons(config).create(
        config, None, apply="factory_reset::factory_reset_device()"
    )

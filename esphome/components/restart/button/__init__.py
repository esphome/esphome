from esphome.components import button
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_RESTART, ENTITY_CATEGORY_CONFIG, ICON_RESTART
from esphome.types import ConfigType

CONFIG_SCHEMA = button.button_schema(
    button.ApplyButton,
    icon=ICON_RESTART,
    device_class=DEVICE_CLASS_RESTART,
    entity_category=ENTITY_CATEGORY_CONFIG,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    await button.sub_buttons(config).create(
        config, None, apply="restart::restart_device()"
    )

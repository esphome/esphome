from esphome.components import switch
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_POWER
from esphome.types import ConfigType

AUTO_LOAD = ["shutdown"]

CONFIG_SCHEMA = switch.switch_schema(
    switch.ApplySwitch,
    icon=ICON_POWER,
    entity_category=ENTITY_CATEGORY_CONFIG,
    block_inverted=True,
)


async def to_code(config: ConfigType) -> None:
    await switch.sub_switches(config).create(
        config, None, apply="shutdown::shutdown_device()", publish="momentary"
    )

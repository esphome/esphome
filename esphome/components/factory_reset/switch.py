from esphome.components import switch
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_RESTART_ALERT
from esphome.types import ConfigType

AUTO_LOAD = ["factory_reset"]

CONFIG_SCHEMA = switch.switch_schema(
    switch.ApplySwitch,
    block_inverted=True,
    icon=ICON_RESTART_ALERT,
    entity_category=ENTITY_CATEGORY_CONFIG,
)


async def to_code(config: ConfigType) -> None:
    await switch.sub_switches(config).create(
        config,
        None,
        apply="factory_reset::factory_reset_device()",
        publish="momentary",
    )

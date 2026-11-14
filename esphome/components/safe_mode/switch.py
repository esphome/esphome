from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_SAFE_MODE, ENTITY_CATEGORY_CONFIG, ICON_RESTART_ALERT
from esphome.types import ConfigType

from . import SafeModeComponent

DEPENDENCIES = ["safe_mode"]

CONFIG_SCHEMA = switch.switch_schema(
    switch.ApplySwitch,
    block_inverted=True,
    entity_category=ENTITY_CATEGORY_CONFIG,
    icon=ICON_RESTART_ALERT,
).extend({cv.GenerateID(CONF_SAFE_MODE): cv.use_id(SafeModeComponent)})


async def to_code(config: ConfigType) -> None:
    switches = switch.sub_switches(config, parent=config[CONF_SAFE_MODE])
    await switches.create(
        config, None, apply="restart_in_safe_mode()", publish="momentary"
    )

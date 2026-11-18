import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_BEEPER, CONF_DISPLAY, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from . import CONF_PIONEER_WT_ID
from .climate import PioneerWT

DEPENDENCIES = ["climate"]

ICON_LED_ON = "mdi:led-on"
ICON_VOLUME_HIGH = "mdi:volume-high"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PIONEER_WT_ID): cv.use_id(PioneerWT),
        cv.Optional(CONF_DISPLAY): switch.switch_schema(
            switch.ApplySwitch,
            icon=ICON_LED_ON,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="DISABLED",
        ),
        cv.Optional(CONF_BEEPER): switch.switch_schema(
            switch.ApplySwitch,
            icon=ICON_VOLUME_HIGH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="DISABLED",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_PIONEER_WT_ID])
    switches = switch.sub_switches(config, parent=hub)
    # No publish: the switch state follows what the AC reports back.
    await switches(CONF_DISPLAY, hub.set_display_switch, apply="set_display_on(state)")
    await switches(CONF_BEEPER, hub.set_beeper_switch, apply="set_beeper_on(state)")

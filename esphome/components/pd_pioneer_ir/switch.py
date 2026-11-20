import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_DISPLAY, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from . import CONF_PD_PIONEER_IR_ID
from .climate import PDPioneerIR

DEPENDENCIES = ["climate"]

ICON_LED_ON = "mdi:led-on"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PD_PIONEER_IR_ID): cv.use_id(PDPioneerIR),
        cv.Optional(CONF_DISPLAY): switch.switch_schema(
            switch.ApplySwitch,
            icon=ICON_LED_ON,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="RESTORE_DEFAULT_ON",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_PD_PIONEER_IR_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches(
        CONF_DISPLAY,
        hub.set_display_switch,
        apply="set_display_on(state)",
        publish="after",
    )

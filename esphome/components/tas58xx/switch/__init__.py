import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INITIAL_STATE,
    CONF_RESTORE_MODE,
    CONF_RESTORE_STATE,
    ENTITY_CATEGORY_CONFIG,
)
from esphome.types import ConfigType

from ..audio_dac import CONF_TAS58XX_ID, TAS58xx

CONF_ACTIVATE = "activate"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_TAS58XX_ID): cv.use_id(TAS58xx),
    # On activates the amplifier (play), off puts it into deep sleep
    cv.Optional(CONF_ACTIVATE): switch.switch_schema(
        switch.ApplySwitch,
        default_restore_mode="DISABLED",
        entity_category=ENTITY_CATEGORY_CONFIG,
    ).extend(
        {
            # The switch shows the state the amplifier publishes, so the restore options have no effect
            cv.Optional(key): cv.invalid(
                "Not supported: the activate switch shows the amplifier's state, which is ON after every boot"
            )
            for key in (CONF_RESTORE_MODE, CONF_RESTORE_STATE, CONF_INITIAL_STATE)
        }
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_TAS58XX_ID])
    switches = switch.sub_switches(config, parent=hub)
    # The amplifier publishes the state (see TAS58xx::publish_active_()), so the switch does not publish it
    await switches(CONF_ACTIVATE, hub.set_activate_switch, apply="set_active(state)")

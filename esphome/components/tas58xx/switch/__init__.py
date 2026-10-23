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

_NOT_RESTORED = "the amplifier always starts in play mode at boot, so the switch state is not restored"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_TAS58XX_ID): cv.use_id(TAS58xx),
    # On activates the amplifier (play), off puts it into deep sleep
    cv.Optional(CONF_ACTIVATE): switch.switch_schema(
        switch.ApplySwitch,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ).extend(
        {
            # The amplifier always starts in play (on), so the state is never saved or restored
            cv.Optional(CONF_RESTORE_MODE, default="ALWAYS_ON"): cv.Any(
                cv.enum(
                    {"ALWAYS_ON": switch.RESTORE_MODES["ALWAYS_ON"]},
                    upper=True,
                    space="_",
                ),
                msg="Only ALWAYS_ON is supported: the amplifier always starts in play mode at boot, "
                "so only ALWAYS_ON is allowed",
            ),
            cv.Optional(CONF_RESTORE_STATE): cv.invalid(
                f"Not supported: {_NOT_RESTORED}"
            ),
            cv.Optional(CONF_INITIAL_STATE): cv.invalid(
                f"Not supported: {_NOT_RESTORED}"
            ),
        }
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_TAS58XX_ID])
    switches = switch.sub_switches(config, parent=hub)
    # The amplifier publishes the state (see TAS58xx::publish_active_()), so the switch does not
    await switches(CONF_ACTIVATE, hub.set_activate_switch, apply="set_active(state)")

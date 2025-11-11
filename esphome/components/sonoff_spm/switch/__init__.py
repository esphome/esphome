"""Switch platform for Sonoff SPM relays."""

import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv

from .. import CONF_SONOFF_SPM_ID, SonoffSPM, sonoff_spm_ns

CONF_RELAY_ID = "relay_id"

SonoffSPMSwitch = sonoff_spm_ns.class_("SonoffSPMSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.switch_schema(SonoffSPMSwitch).extend(
    {
        cv.GenerateID(CONF_SONOFF_SPM_ID): cv.use_id(SonoffSPM),
        cv.Required(CONF_RELAY_ID): cv.int_range(min=0, max=127),
    }
)


async def to_code(config):
    """Generate code for Sonoff SPM switch."""
    var = await switch.new_switch(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_SONOFF_SPM_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_relay_id(config[CONF_RELAY_ID]))
    cg.add(parent.register_switch(var, config[CONF_RELAY_ID]))

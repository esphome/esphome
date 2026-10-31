import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_M5STACK_4RELAY_ID, DOMAIN, M5Stack4Relay

DEPENDENCIES = ["m5stack_4relay"]

CONFIG_SCHEMA = switch.switch_schema(
    switch.ApplySwitch, default_restore_mode="DISABLED"
).extend(
    {
        cv.GenerateID(CONF_M5STACK_4RELAY_ID): cv.use_id(M5Stack4Relay),
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=4),
    }
)


# The hub has one slot per channel, so a second switch on it would take over the first
FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain=switch.DOMAIN,
    platform=DOMAIN,
    keys=(CONF_M5STACK_4RELAY_ID, CONF_CHANNEL),
    message="Channel {channel} of {m5stack_4relay_id} is used by more than one switch",
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_M5STACK_4RELAY_ID])
    channel = config[CONF_CHANNEL] - 1
    await switch.sub_switches(config, parent=hub).create(
        config,
        lambda sw: hub.register_switch(channel, sw),
        apply=f"set_relay({channel}, state)",
    )

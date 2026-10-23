import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_SEEED_MULTI_CHANNEL_RELAY_ID, DOMAIN, SeeedMultiChannelRelay

DEPENDENCIES = ["seeed_multi_channel_relay"]

CONFIG_SCHEMA = switch.switch_schema(
    switch.ApplySwitch, default_restore_mode="RESTORE_DEFAULT_OFF"
).extend(
    {
        cv.GenerateID(CONF_SEEED_MULTI_CHANNEL_RELAY_ID): cv.use_id(
            SeeedMultiChannelRelay
        ),
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=8),
    }
)


# The hub has one slot per channel, so a second switch on it would take over the first
FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain=switch.DOMAIN,
    platform=DOMAIN,
    keys=(CONF_SEEED_MULTI_CHANNEL_RELAY_ID, CONF_CHANNEL),
    message="Channel {1} of {0} is used by more than one switch",
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SEEED_MULTI_CHANNEL_RELAY_ID])
    channel = config[CONF_CHANNEL] - 1
    await switch.sub_switches(config, parent=hub).create(
        config,
        lambda sw: hub.register_switch(channel, sw),
        apply=f"set_relay({channel}, state)",
    )

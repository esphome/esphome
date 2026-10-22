import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_PLATFORM
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_SEEED_MULTI_CHANNEL_RELAY_ID, DOMAIN, SeeedMultiChannelRelay

DEPENDENCIES = ["seeed_multi_channel_relay"]

CONFIG_SCHEMA = switch.switch_schema(switch.ApplySwitch).extend(
    {
        cv.GenerateID(CONF_SEEED_MULTI_CHANNEL_RELAY_ID): cv.use_id(
            SeeedMultiChannelRelay
        ),
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=8),
    }
)


def _validate_unique_channel(config: ConfigType) -> ConfigType:
    # The hub has one slot per channel, so a second switch on it would take over the first
    hub, channel = config[CONF_SEEED_MULTI_CHANNEL_RELAY_ID], config[CONF_CHANNEL]
    users = [
        other
        for other in fv.full_config.get().get(switch.DOMAIN, [])
        if other.get(CONF_PLATFORM) == DOMAIN
        and other[CONF_SEEED_MULTI_CHANNEL_RELAY_ID] == hub
        and other[CONF_CHANNEL] == channel
    ]
    if len(users) > 1:
        raise cv.Invalid(
            f"Channel {channel} of {hub} is used by more than one switch",
            path=[CONF_CHANNEL],
        )
    return config


FINAL_VALIDATE_SCHEMA = _validate_unique_channel


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SEEED_MULTI_CHANNEL_RELAY_ID])
    channel = config[CONF_CHANNEL] - 1
    await switch.sub_switches(config, parent=hub).create(
        config,
        lambda sw: hub.register_switch(channel, sw),
        apply=f"set_relay({channel}, state)",
    )

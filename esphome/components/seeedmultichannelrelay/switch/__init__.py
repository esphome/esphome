import esphome.codegen as cg
from esphome.components import i2c as i2c, switch
import esphome.config_validation as cv
from esphome.const import CONF_INTERLOCK, CONF_CHANNEL

from .. import (
    CONF_SEEED_MULTI_CHANNEL_RELAY_ID,
    Seeed_Multi_Channel_Relay as Seeed_Multi_Channel_Relay,
    seeed_multi_channel_relay_ns,
)

DEPENDENCIES = ["seeed_multi_channel_relay"]

Seeed_Multi_Channel_Relay_Switch = seeed_multi_channel_relay_ns.class_(
    "Seeed_Multi_Channel_Relay_Switch", cg.Component, switch.Switch
)

CONF_INTERLOCK_WAIT_TIME = "interlock_wait_time"

RelayBit_ = seeed_multi_channel_relay_ns.enum("RelayBit", is_class=True)

CONFIG_SCHEMA = (
    switch.switch_schema(Seeed_Multi_Channel_Relay_Switch)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(Seeed_Multi_Channel_Relay_Switch),
            cv.GenerateID(CONF_SEEED_MULTI_CHANNEL_RELAY_ID): cv.use_id(
                SeeedMultiChannelRelay
            ),
            cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=8),
            cv.Optional(CONF_INTERLOCK): cv.ensure_list(cv.use_id(switch.Switch)),
            cv.Optional(
                CONF_INTERLOCK_WAIT_TIME, default="0ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SEEED_MULTI_CHANNEL_RELAY_ID])

    cg.add(var.set_channel(config[CONF_CHANNEL]))
    if CONF_INTERLOCK in config:
        cg.add_define("USE_SWITCH_INTERLOCK")
        interlock = []
        for it in config[CONF_INTERLOCK]:
            lock = await cg.get_variable(it)
            interlock.append(lock)
        cg.add(var.set_interlock(interlock))
        cg.add(var.set_interlock_wait_time(config[CONF_INTERLOCK_WAIT_TIME]))

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, switch
from esphome.const import CONF_CHANNEL, CONF_INTERLOCK

from .. import (
    seeedmultichannelrelay_ns,
    SeeedMultiChannelRelay,
    CONF_SEEEDMULTICHANNELRELAY_ID,
)

DEPENDENCIES = ["seeedmultichannelrelay"]

SeeedMultiChannelRelaySwitch = seeedmultichannelrelay_ns.class_(
    "SeeedMultiChannelRelaySwitch", cg.Component, i2c.I2CDevice, switch.Switch
)

CONF_INTERLOCK_WAIT_TIME = "interlock_wait_time"

CONF_Relay_1 = 1
CONF_Relay_2 = 2
CONF_Relay_3 = 3
CONF_Relay_4 = 4
CONF_Relay_5 = 5
CONF_Relay_6 = 6
CONF_Relay_7 = 7
CONF_Relay_8 = 8

RelayBit_ = seeedmultichannelrelay_ns.enum("RelayBit", is_class=True)

SWITCH_MAP = {
    CONF_Relay_1: RelayBit_.RELAY1,
    CONF_Relay_2: RelayBit_.RELAY2,
    CONF_Relay_3: RelayBit_.RELAY3,
    CONF_Relay_4: RelayBit_.RELAY4,
    CONF_Relay_5: RelayBit_.RELAY5,
    CONF_Relay_6: RelayBit_.RELAY6,
    CONF_Relay_7: RelayBit_.RELAY7,
    CONF_Relay_8: RelayBit_.RELAY8,
}


CONFIG_SCHEMA = (
    switch.switch_schema(SeeedMultiChannelRelaySwitch)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(SeeedMultiChannelRelaySwitch),
            cv.GenerateID(CONF_SEEEDMULTICHANNELRELAY_ID): cv.use_id(
                SeeedMultiChannelRelay
            ),
            cv.Required(CONF_CHANNEL): cv.enum(SWITCH_MAP),
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
    await cg.register_parented(var, config[CONF_SEEEDMULTICHANNELRELAY_ID])

    cg.add(var.set_channel(config[CONF_CHANNEL]))

    if CONF_INTERLOCK in config:
        interlock = []
        for it in config[CONF_INTERLOCK]:
            lock = await cg.get_variable(it)
            interlock.append(lock)
        cg.add(var.set_interlock(interlock))
        cg.add(var.set_interlock_wait_time(config[CONF_INTERLOCK_WAIT_TIME]))

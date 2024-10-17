import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, switch
from esphome.const import CONF_CHANNEL

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

RelayBit_ = seeedmultichannelrelay_ns.enum("RelayBit", is_class=True)

CONFIG_SCHEMA = (
    switch.switch_schema(SeeedMultiChannelRelaySwitch)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(SeeedMultiChannelRelaySwitch),
            cv.GenerateID(CONF_SEEEDMULTICHANNELRELAY_ID): cv.use_id(
                SeeedMultiChannelRelay
            ),
            cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=8),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SEEEDMULTICHANNELRELAY_ID])

    cg.add(var.set_channel(config[CONF_CHANNEL]))

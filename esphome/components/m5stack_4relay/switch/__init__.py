import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, switch
from esphome.const import CONF_CHANNEL

from .. import m5stack_4relay_ns, M5Stack_4Relay, CONF_M5STACK_4RELAY_ID

DEPENDENCIES = ["m5stack_4relay"]

M5StackSwitch = m5stack_4relay_ns.class_(
    "M5Stack4RelaySwitch", cg.Component, i2c.I2CDevice, switch.Switch
)

CONF_INTERLOCK_WAIT_TIME = "interlock_wait_time"

CONFIG_SCHEMA = (
    switch.switch_schema(M5StackSwitch)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(M5StackSwitch),
            cv.GenerateID(CONF_M5STACK_4RELAY_ID): cv.use_id(M5Stack_4Relay),
            cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=4),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_M5STACK_4RELAY_ID])

    cg.add(var.set_channel(config[CONF_CHANNEL]))

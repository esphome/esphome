import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, switch
from esphome.const import CONF_CHANNEL  # CONF_INTERLOCK, CONF_ID,

from .. import M5Stack_ns, M5Stack_4_Relays, CONF_M5Stack_4_Relays_ID

DEPENDENCIES = ["m5stack_4_relays"]

M5StackSwitch = M5Stack_ns.class_(
    "M5Stack_Switch", cg.Component, i2c.I2CDevice, switch.Switch
)

CONF_INTERLOCK_WAIT_TIME = "interlock_wait_time"

CONF_Relay_1 = "Relay_1"
CONF_Relay_2 = "Relay_2"
CONF_Relay_3 = "Relay_3"
CONF_Relay_4 = "Relay_4"

CONF_INTERLOCK_WAIT_TIME = "interlock_wait_time"

RelayBit_ = M5Stack_ns.enum("RelayBit", is_class=True)

SWITCH_MAP = {
    CONF_Relay_1: RelayBit_.RELAY1,
    CONF_Relay_2: RelayBit_.RELAY2,
    CONF_Relay_3: RelayBit_.RELAY3,
    CONF_Relay_4: RelayBit_.RELAY4,
}


CONFIG_SCHEMA = (
    switch.switch_schema(M5StackSwitch)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(M5StackSwitch),
            cv.GenerateID(CONF_M5Stack_4_Relays_ID): cv.use_id(M5Stack_4_Relays),
            cv.Required(CONF_CHANNEL): cv.enum(SWITCH_MAP),
            # cv.Optional(CONF_INTERLOCK): cv.ensure_list(cv.use_id(switch.Switch)),
            # cv.Optional(CONF_INTERLOCK_WAIT_TIME, default="0ms"): cv.positive_time_period_milliseconds
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    # var = await cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    # await switch.register_switch(var, config)
    await cg.register_parented(var, config[CONF_M5Stack_4_Relays_ID])

    cg.add(var.set_channel(config[CONF_CHANNEL]))

    # if CONF_INTERLOCK in config:
    #    interlock = []
    #    for it in config[CONF_INTERLOCK]:
    #        lock = await cg.get_variable(it)
    #        interlock.append(lock)
    #    cg.add(var.set_interlock(interlock))
    #    cg.add(var.set_interlock_wait_time(config[CONF_INTERLOCK_WAIT_TIME]))

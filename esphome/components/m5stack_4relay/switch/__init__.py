import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL
from esphome.types import ConfigType

from .. import CONF_M5STACK_4RELAY_ID, M5Stack4Relay

DEPENDENCIES = ["m5stack_4relay"]

CONFIG_SCHEMA = switch.switch_schema(
    switch.ApplySwitch, default_restore_mode="DISABLED"
).extend(
    {
        cv.GenerateID(CONF_M5STACK_4RELAY_ID): cv.use_id(M5Stack4Relay),
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=4),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_M5STACK_4RELAY_ID])
    channel = config[CONF_CHANNEL] - 1
    var = await switch.sub_switches(config, parent=hub).create(
        config, None, apply=f"set_relay({channel}, state)"
    )
    cg.add(hub.register_switch(channel, var))

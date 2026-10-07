import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv

from .cover import Tormatic, tormatic_ns

TormaticSwitch = tormatic_ns.class_("TormaticSwitch", switch.Switch)

CONF_TORMATIC_ID = "tormatic_id"

CONFIG_SCHEMA = switch.switch_schema(TormaticSwitch).extend(
    {
        cv.Required(CONF_TORMATIC_ID): cv.use_id(Tormatic),
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_TORMATIC_ID])
    var = await switch.new_switch(config, paren)
    cg.add(paren.set_light_switch(var))

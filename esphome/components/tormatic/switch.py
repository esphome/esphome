import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv

from . import Tormatic, tormatic_ns

CONF_TORMATIC_ID = "tormatic_id"

TormaticLightSwitch = tormatic_ns.class_(
    "TormaticLightSwitch",
    switch.Switch,
    cg.Parented.template(Tormatic),
)

CONFIG_SCHEMA = switch.switch_schema(TormaticLightSwitch).extend(
    {
        cv.GenerateID(CONF_TORMATIC_ID): cv.use_id(Tormatic),
    }
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_parented(var, config[CONF_TORMATIC_ID])

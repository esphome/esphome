from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg
from . import IEC62056Component, CONF_IEC62056_ID, iec62056_ns

IEC62056Switch = iec62056_ns.class_("IEC62056Switch", switch.Switch)

CONFIG_SCHEMA = switch.switch_schema(IEC62056Switch).extend(
    {
        cv.GenerateID(CONF_IEC62056_ID): cv.use_id(IEC62056Component),
    }
)


async def to_code(config):
    var = await switch.new_switch(config)
    parent = await cg.get_variable(config[CONF_IEC62056_ID])
    cg.add(var.set_parent(parent))

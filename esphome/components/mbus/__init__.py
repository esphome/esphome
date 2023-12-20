import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

mbus_ns = cg.esphome_ns.namespace("mbus")
MBus = mbus_ns.class_("MBus", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MBus),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    cg.add_global(mbus_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

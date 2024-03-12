import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from ..sensor import bh1745_ns, BH1745SComponent, CONF_BH1745_ID

BH1745SwitchLed = bh1745_ns.class_("BH1745SwitchLed", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(BH1745SwitchLed),
        cv.GenerateID(CONF_BH1745_ID): cv.use_id(BH1745SComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    hub = await cg.get_variable(config[CONF_BH1745_ID])
    cg.add(var.set_bh1745(hub))

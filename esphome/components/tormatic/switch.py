import esphome.codegen as cg
from esphome.components import cover, switch
import esphome.config_validation as cv

tormatic_ns = cg.esphome_ns.namespace("tormatic")
Tormatic = tormatic_ns.class_("Tormatic", cover.Cover, cg.PollingComponent)
TormaticSwitch = tormatic_ns.class_("TormaticSwitch", switch.Switch, cg.Component)

CONF_TORMATIC_ID = "tormatic_id"

CONFIG_SCHEMA = (
    switch.switch_schema(TormaticSwitch)
    .extend(
        {
            cv.Required(CONF_TORMATIC_ID): cv.use_id(Tormatic),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    paren = await cg.get_variable(config[CONF_TORMATIC_ID])
    cg.add(var.set_parent(paren))

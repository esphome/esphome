import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_KEY

from .. import CONF_CLUSTER, CONF_MIIO_ID, Miio, miio_ns

DEPENDENCIES = ["miio"]

MiioSwitch = miio_ns.class_("MiioSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = (
    switch.switch_schema(MiioSwitch)
    .extend(
        {
            cv.GenerateID(CONF_MIIO_ID): cv.use_id(Miio),
            cv.Required(CONF_CLUSTER): cv.uint8_t,
            cv.Required(CONF_KEY): cv.uint8_t,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    parent = await cg.get_variable(config[CONF_MIIO_ID])

    cg.add(var.set_miio_parent(parent))
    cg.add(var.set_switch_id(config[CONF_CLUSTER], config[CONF_KEY]))

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.components.jablotron import (
    ACCESS_CODE_SCHEMA,
    INDEX_SCHEMA,
    JABLOTRON_DEVICE_SCHEMA,
    set_access_code,
    set_index,
    register_jablotron_device,
)

jablotron_pg_ns = cg.esphome_ns.namespace("jablotron_pg")
PGSwitch = jablotron_pg_ns.class_("PGSwitch", switch.Switch)

DEPENDENCIES = ["jablotron", "switch"]
CONFIG_SCHEMA = (
    switch.SWITCH_SCHEMA.extend(JABLOTRON_DEVICE_SCHEMA)
    .extend(ACCESS_CODE_SCHEMA)
    .extend(INDEX_SCHEMA)
    .extend({cv.GenerateID(): cv.declare_id(PGSwitch)})
)


async def to_code(config):
    var = cg.new_Pvariable(config[cv.CONF_ID])
    await switch.register_switch(var, config)
    set_access_code(var, config)
    set_index(var, config)
    await register_jablotron_device(var, config)

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_SWITCH_DATAPOINT

from .. import (
    CONF_ECONET_ID,
    CONF_REQUEST_MOD,
    CONF_REQUEST_ONCE,
    CONF_SRC_ADDRESS,
    ECONET_CLIENT_SCHEMA,
    EconetClient,
    econet_ns,
)

DEPENDENCIES = ["econet"]

EconetSwitch = econet_ns.class_(
    "EconetSwitch", switch.Switch, cg.Component, EconetClient
)

CONFIG_SCHEMA = (
    switch.switch_schema(EconetSwitch)
    .extend(
        {
            cv.Required(CONF_SWITCH_DATAPOINT): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ECONET_CLIENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)

    paren = await cg.get_variable(config[CONF_ECONET_ID])
    cg.add(var.set_econet_parent(paren))
    cg.add(var.set_request_mod(config[CONF_REQUEST_MOD]))
    cg.add(var.set_request_once(config[CONF_REQUEST_ONCE]))
    cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))
    cg.add(var.set_src_adr(config[CONF_SRC_ADDRESS]))

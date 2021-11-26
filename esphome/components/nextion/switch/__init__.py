import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from esphome.const import CONF_ID
from .. import nextion_ns, CONF_NEXTION_ID

from ..base_component import (
    setup_component_core_,
    CONF_COMPONENT_NAME,
    CONF_VARIABLE_NAME,
    CONFIG_SWITCH_COMPONENT_SCHEMA,
)

CODEOWNERS = ["@senexcrenshaw"]

NextionSwitch = nextion_ns.class_("NextionSwitch", switch.Switch, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(
    switch.SWITCH_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(NextionSwitch),
        }
    )
    .extend(CONFIG_SWITCH_COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("never")),
    cv.has_exactly_one_key(CONF_COMPONENT_NAME, CONF_VARIABLE_NAME),
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NEXTION_ID])
    var = cg.new_Pvariable(config[CONF_ID], hub)
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    cg.add(hub.register_switch_component(var))

    await setup_component_core_(var, config, ".val")

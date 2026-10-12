from esphome import automation
import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_STATE

from .. import CONF_NEXTION_ID, CONF_PUBLISH_STATE, CONF_SEND_TO_NEXTION, nextion_ns
from ..base_component import (
    CONF_COMPONENT_NAME,
    CONF_VARIABLE_NAME,
    CONFIG_SWITCH_COMPONENT_SCHEMA,
    setup_component_core_,
)

CODEOWNERS = ["@senexcrenshaw"]

NextionSwitch = nextion_ns.class_("NextionSwitch", switch.Switch, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(
    switch.switch_schema(NextionSwitch)
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


automation.register_apply_action(
    "switch.nextion.publish",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(NextionSwitch),
            cv.Required(CONF_STATE): cv.templatable(cv.boolean),
            cv.Optional(CONF_PUBLISH_STATE, default="true"): cv.templatable(cv.boolean),
            cv.Optional(CONF_SEND_TO_NEXTION, default="true"): cv.templatable(
                cv.boolean
            ),
        }
    ),
    automation.ApplyCall(
        "set_state({}, {}, {})",
        (
            (CONF_STATE, cg.bool_),
            (CONF_PUBLISH_STATE, cg.bool_),
            (CONF_SEND_TO_NEXTION, cg.bool_),
        ),
    ),
)

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from esphome.const import CONF_ID, CONF_STATE

from .. import nextion_ns, CONF_NEXTION_ID, CONF_PUBLISH_STATE, CONF_SEND_TO_NEXTION

from ..base_component import (
    setup_component_core_,
    CONF_COMPONENT_NAME,
    CONF_VARIABLE_NAME,
    CONFIG_SWITCH_COMPONENT_SCHEMA,
)

CODEOWNERS = ["@senexcrenshaw"]

NextionSwitch = nextion_ns.class_("NextionSwitch", switch.Switch, cg.PollingComponent)

NextionPublishBoolAction = nextion_ns.class_(
    "NextionPublishBoolAction", automation.Action
)

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


@automation.register_action(
    "switch.nextion.publish",
    NextionPublishBoolAction,
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
)
async def sensor_nextion_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_STATE], args, bool)
    cg.add(var.set_state(template_))

    template_ = await cg.templatable(config[CONF_PUBLISH_STATE], args, bool)
    cg.add(var.set_publish_state(template_))

    template_ = await cg.templatable(config[CONF_SEND_TO_NEXTION], args, bool)
    cg.add(var.set_send_to_nextion(template_))

    return var

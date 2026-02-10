from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@yourusername"]  # Replace with your GitHub username

filter_lifetime_ns = cg.esphome_ns.namespace("filter_lifetime")
FilterLifetime = filter_lifetime_ns.class_("FilterLifetime")

ResetFilterAction = filter_lifetime_ns.class_("ResetFilterAction", automation.Action)


@automation.register_action(
    "filter_lifetime.reset_filter",
    ResetFilterAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(FilterLifetime),
        }
    ),
)
async def reset_filter_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)

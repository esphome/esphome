import voluptuous as vol

from esphomeyaml import automation
from esphomeyaml.automation import ACTION_REGISTRY, maybe_simple_id
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID
from esphomeyaml.helpers import NoArg, Pvariable, TemplateArguments, esphomelib_ns, get_variable

Script = esphomelib_ns.Script
ScriptExecuteAction = esphomelib_ns.ScriptExecuteAction

CONFIG_SCHEMA = automation.validate_automation({
    vol.Required(CONF_ID): cv.declare_variable_id(Script),
})


def to_code(config):
    for conf in config:
        trigger = Pvariable(conf[CONF_ID], Script.new())
        automation.build_automation(trigger, NoArg, conf)


CONF_SCRIPT_EXECUTE = 'script.execute'
SCRIPT_EXECUTE_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(Script),
})


@ACTION_REGISTRY.register(CONF_SCRIPT_EXECUTE, SCRIPT_EXECUTE_ACTION_SCHEMA)
def script_execute_action_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_execute_action(template_arg)
    type = ScriptExecuteAction.template(arg_type)
    yield Pvariable(action_id, rhs, type=type)

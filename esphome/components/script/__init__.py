import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.const import CONF_ID

script_ns = cg.esphome_ns.namespace('script')
Script = script_ns.class_('Script', automation.Trigger.template())
ScriptExecuteAction = script_ns.class_('ScriptExecuteAction', automation.Action)
ScriptStopAction = script_ns.class_('ScriptStopAction', automation.Action)
ScriptWaitAction = script_ns.class_('ScriptWaitAction', automation.Action)
IsRunningCondition = script_ns.class_('IsRunningCondition', automation.Condition)

CONFIG_SCHEMA = automation.validate_automation({
    cv.Required(CONF_ID): cv.declare_id(Script),
})


def to_code(config):
    # Register all variables first, so that scripts can use other scripts
    triggers = []
    for conf in config:
        trigger = cg.new_Pvariable(conf[CONF_ID])
        triggers.append((trigger, conf))

    for trigger, conf in triggers:
        yield automation.build_automation(trigger, [], conf)


@automation.register_action('script.execute', ScriptExecuteAction, maybe_simple_id({
    cv.Required(CONF_ID): cv.use_id(Script),
}))
def script_execute_action_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action('script.stop', ScriptStopAction, maybe_simple_id({
    cv.Required(CONF_ID): cv.use_id(Script)
}))
def script_stop_action_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action('script.wait', ScriptWaitAction, maybe_simple_id({
    cv.Required(CONF_ID): cv.use_id(Script)
}))
def script_wait_action_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_condition('script.is_running', IsRunningCondition, automation.maybe_simple_id({
    cv.Required(CONF_ID): cv.use_id(Script)
}))
def script_is_running_to_code(config, condition_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(condition_id, template_arg, paren)

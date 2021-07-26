import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.const import CONF_ID, CONF_MODE

CODEOWNERS = ["@esphome/core"]
script_ns = cg.esphome_ns.namespace("script")
Script = script_ns.class_("Script", automation.Trigger.template())
ScriptExecuteAction = script_ns.class_("ScriptExecuteAction", automation.Action)
ScriptStopAction = script_ns.class_("ScriptStopAction", automation.Action)
ScriptWaitAction = script_ns.class_("ScriptWaitAction", automation.Action, cg.Component)
IsRunningCondition = script_ns.class_("IsRunningCondition", automation.Condition)
SingleScript = script_ns.class_("SingleScript", Script)
RestartScript = script_ns.class_("RestartScript", Script)
QueueingScript = script_ns.class_("QueueingScript", Script, cg.Component)
ParallelScript = script_ns.class_("ParallelScript", Script)

CONF_SINGLE = "single"
CONF_RESTART = "restart"
CONF_QUEUED = "queued"
CONF_PARALLEL = "parallel"
CONF_MAX_RUNS = "max_runs"

SCRIPT_MODES = {
    CONF_SINGLE: SingleScript,
    CONF_RESTART: RestartScript,
    CONF_QUEUED: QueueingScript,
    CONF_PARALLEL: ParallelScript,
}


def check_max_runs(value):
    if CONF_MAX_RUNS not in value:
        return value
    if value[CONF_MODE] not in [CONF_QUEUED, CONF_PARALLEL]:
        raise cv.Invalid(
            "The option 'max_runs' is only valid in 'queue' and 'parallel' mode.",
            path=[CONF_MAX_RUNS],
        )
    return value


def assign_declare_id(value):
    value = value.copy()
    value[CONF_ID] = cv.declare_id(SCRIPT_MODES[value[CONF_MODE]])(value[CONF_ID])
    return value


CONFIG_SCHEMA = automation.validate_automation(
    {
        # Don't declare id as cv.declare_id yet, because the ID type
        # depends on the mode. Will be checked later with assign_declare_id
        cv.Required(CONF_ID): cv.string_strict,
        cv.Optional(CONF_MODE, default=CONF_SINGLE): cv.one_of(
            *SCRIPT_MODES, lower=True
        ),
        cv.Optional(CONF_MAX_RUNS): cv.positive_int,
    },
    extra_validators=cv.All(check_max_runs, assign_declare_id),
)


async def to_code(config):
    # Register all variables first, so that scripts can use other scripts
    triggers = []
    for conf in config:
        trigger = cg.new_Pvariable(conf[CONF_ID])
        # Add a human-readable name to the script
        cg.add(trigger.set_name(conf[CONF_ID].id))

        if CONF_MAX_RUNS in conf:
            cg.add(trigger.set_max_runs(conf[CONF_MAX_RUNS]))

        if conf[CONF_MODE] == CONF_QUEUED:
            await cg.register_component(trigger, conf)

        triggers.append((trigger, conf))

    for trigger, conf in triggers:
        await automation.build_automation(trigger, [], conf)


@automation.register_action(
    "script.execute",
    ScriptExecuteAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(Script),
        }
    ),
)
async def script_execute_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "script.stop",
    ScriptStopAction,
    maybe_simple_id({cv.Required(CONF_ID): cv.use_id(Script)}),
)
async def script_stop_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "script.wait",
    ScriptWaitAction,
    maybe_simple_id({cv.Required(CONF_ID): cv.use_id(Script)}),
)
async def script_wait_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    await cg.register_component(var, {})
    return var


@automation.register_condition(
    "script.is_running",
    IsRunningCondition,
    automation.maybe_simple_id({cv.Required(CONF_ID): cv.use_id(Script)}),
)
async def script_is_running_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)

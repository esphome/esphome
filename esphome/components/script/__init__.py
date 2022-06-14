import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.const import CONF_ID, CONF_MODE, CONF_PARAMETERS
from esphome.core import CORE

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

CONF_SCRIPT = "script"
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

# TODO add support for entity ids as params
SCRIPT_PARAMS_NATIVE_TYPES = {
    "bool": bool,
    "int": cg.int32,
    "float": float,
    "string": cg.std_string,
    "bool[]": cg.std_vector.template(bool),
    "int[]": cg.std_vector.template(cg.int32),
    "float[]": cg.std_vector.template(float),
    "string[]": cg.std_vector.template(cg.std_string),
}


def get_script(script_id):
    scripts = CORE.config.get(CONF_SCRIPT, {})
    for script in scripts:
        if script.get(CONF_ID, None) == script_id:
            return script
    raise cv.Invalid(f"Script id '{script_id}' not found")


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


def parameters_to_template(args):
    template_args = []
    func_args = []
    script_arg_names = []
    for name, type_ in args.items():
        native_type = SCRIPT_PARAMS_NATIVE_TYPES[type_]
        template_args.append(native_type)
        func_args.append((native_type, name))
        script_arg_names.append(name)
    template = cg.TemplateArguments(*template_args)
    return template, func_args


CONFIG_SCHEMA = automation.validate_automation(
    {
        # Don't declare id as cv.declare_id yet, because the ID type
        # depends on the mode. Will be checked later with assign_declare_id
        cv.Required(CONF_ID): cv.string_strict,
        cv.Optional(CONF_MODE, default=CONF_SINGLE): cv.one_of(
            *SCRIPT_MODES, lower=True
        ),
        cv.Optional(CONF_MAX_RUNS): cv.positive_int,
        cv.Optional(CONF_PARAMETERS, default={}): cv.Schema(
            {
                cv.validate_id_name: cv.one_of(*SCRIPT_PARAMS_NATIVE_TYPES, lower=True),
            }
        ),
    },
    extra_validators=cv.All(check_max_runs, assign_declare_id),
)


async def to_code(config):
    # Register all variables first, so that scripts can use other scripts
    triggers = []
    for conf in config:
        template, func_args = parameters_to_template(conf[CONF_PARAMETERS])
        trigger = cg.new_Pvariable(conf[CONF_ID], template)
        # Add a human-readable name to the script
        cg.add(trigger.set_name(conf[CONF_ID].id))

        if CONF_MAX_RUNS in conf:
            cg.add(trigger.set_max_runs(conf[CONF_MAX_RUNS]))

        if conf[CONF_MODE] == CONF_QUEUED:
            await cg.register_component(trigger, conf)

        triggers.append((trigger, func_args, conf))

    for trigger, func_args, conf in triggers:
        await automation.build_automation(trigger, func_args, conf)


@automation.register_action(
    "script.execute",
    ScriptExecuteAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(Script),
            cv.Optional(cv.validate_id_name): cv.templatable(cv.valid),
        },
    ),
)
async def script_execute_action_to_code(config, action_id, template_arg, args):
    async def get_ordered_args(config, script_params):
        config_args = config.copy()
        config_args.pop(CONF_ID)

        # match script_args to the formal parameter order
        script_args = []
        for _, name in script_params:
            if name not in config_args:
                raise cv.Invalid(
                    f"Missing parameter: '{name}' in script.execute {config[CONF_ID]}"
                )
            arg = await cg.templatable(config_args[name], args, None)
            script_args.append(arg)
        return script_args

    script = get_script(config[CONF_ID])
    params = script.get(CONF_PARAMETERS, [])
    template, script_params = parameters_to_template(params)
    script_args = await get_ordered_args(config, script_params)

    # We need to use the parent class 'Script' as the template argument
    # to match the partial specialization of the ScriptExecuteAction template
    template_arg = cg.TemplateArguments(Script.template(template), *template_arg)

    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    cg.add(var.set_args(*script_args))
    return var


@automation.register_action(
    "script.stop",
    ScriptStopAction,
    maybe_simple_id({cv.Required(CONF_ID): cv.use_id(Script)}),
)
async def script_stop_action_to_code(config, action_id, template_arg, args):
    full_id, paren = await cg.get_variable_with_full_id(config[CONF_ID])
    template_arg = cg.TemplateArguments(full_id.type, *template_arg)
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "script.wait",
    ScriptWaitAction,
    maybe_simple_id({cv.Required(CONF_ID): cv.use_id(Script)}),
)
async def script_wait_action_to_code(config, action_id, template_arg, args):
    full_id, paren = await cg.get_variable_with_full_id(config[CONF_ID])
    template_arg = cg.TemplateArguments(full_id.type, *template_arg)
    var = cg.new_Pvariable(action_id, template_arg, paren)
    await cg.register_component(var, {})
    return var


@automation.register_condition(
    "script.is_running",
    IsRunningCondition,
    automation.maybe_simple_id({cv.Required(CONF_ID): cv.use_id(Script)}),
)
async def script_is_running_to_code(config, condition_id, template_arg, args):
    full_id, paren = await cg.get_variable_with_full_id(config[CONF_ID])
    template_arg = cg.TemplateArguments(full_id.type, *template_arg)
    return cg.new_Pvariable(condition_id, template_arg, paren)

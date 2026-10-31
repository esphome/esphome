from dataclasses import dataclass, field
from typing import Any

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components.const import CONF_ON_STOP
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE, CONF_PARAMETERS, CONF_RESTART
from esphome.core import CORE, ID, EsphomeError
from esphome.cpp_generator import MockObj, SafeExpType, Statement
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
DOMAIN = "script"
script_ns = cg.esphome_ns.namespace("script")
Script = script_ns.class_("Script")
AutomationScript = script_ns.class_(
    "AutomationScript", Script, automation.Trigger.template()
)
FunctionScript = script_ns.class_("FunctionScript", Script)
ScriptWaitAction = script_ns.class_("ScriptWaitAction", automation.Action, cg.Component)
SingleScript = script_ns.class_("SingleScript", AutomationScript)
RestartScript = script_ns.class_("RestartScript", AutomationScript)
QueueingScript = script_ns.class_("QueueingScript", AutomationScript, cg.Component)
ParallelScript = script_ns.class_("ParallelScript", AutomationScript)

CONF_SINGLE = "single"
CONF_QUEUED = "queued"
CONF_PARALLEL = "parallel"
CONF_MAX_RUNS = "max_runs"
CONF_SCRIPT_STOP = "script.stop"

SCRIPT_MODES = {
    CONF_SINGLE: SingleScript,
    CONF_RESTART: RestartScript,
    CONF_QUEUED: QueueingScript,
    CONF_PARALLEL: ParallelScript,
}
# The same modes, templated on the class that runs the actions
MODE_TEMPLATES = {
    CONF_SINGLE: script_ns.class_("SingleMode", Script),
    CONF_RESTART: script_ns.class_("RestartMode", Script),
    CONF_QUEUED: script_ns.class_("QueueingMode", Script, cg.Component),
    CONF_PARALLEL: script_ns.class_("ParallelMode", Script),
}


@dataclass
class ScriptData:
    # The statements that name a script's class, by script id, until its class is final
    pending: dict[str, list[Statement]] = field(default_factory=dict)


def _data() -> ScriptData:
    return CORE.data.setdefault(DOMAIN, ScriptData())


def _new_pvariable(id_: ID, *args: SafeExpType) -> tuple[MockObj, list[Statement]]:
    """``cg.new_Pvariable``, also returning the global and setup statements it added."""
    globals_, main = len(CORE.global_statements), len(CORE.main_statements)
    result = cg.new_Pvariable(id_, *args)
    return result, [*CORE.global_statements[globals_:], *CORE.main_statements[main:]]


def _retype(statements: list[Statement], old: str, new: str) -> None:
    """Replace ``old`` with ``new`` in ``statements``, where they are."""
    ids = {id(statement) for statement in statements}
    for section in (CORE.global_statements, CORE.main_statements):
        for index, statement in enumerate(section):
            if id(statement) in ids:
                section[index] = cg.RawStatement(str(statement).replace(old, new))


def _use_function(
    var: MockObj, mode: str, func_args: list[tuple[Any, str]], fn: MockObj
) -> None:
    """Make script ``var`` a ``FunctionScript`` that calls ``fn``, where it was declared."""
    full_id = var.base
    types = [type_ for type_, _ in func_args]
    new_type = MODE_TEMPLATES[mode].template(
        FunctionScript.template(fn, *types), *types
    )
    statements = _data().pending.pop(full_id.id)
    # The function was declared last; the class names it, so it moves before the script
    declaration = CORE.global_statements.pop()
    CORE.global_statements.insert(
        next(
            index
            for index, statement in enumerate(CORE.global_statements)
            if statement is statements[0]
        ),
        declaration,
    )
    _retype(statements, str(full_id.type), str(new_type))
    full_id.type = new_type


PARAMETER_TYPE_TRANSLATIONS = {
    "string": "std::string",
    "boolean": "bool",
}


def get_script(script_id):
    scripts = CORE.config.get(DOMAIN, {})
    for script in scripts:
        if script.get(CONF_ID, None) == script_id:
            return script
    raise cv.Invalid(f"Script id '{script_id}' not found")


def check_max_runs(value):
    # Set default for queued mode to prevent unbounded queue growth
    if CONF_MAX_RUNS not in value and value[CONF_MODE] == CONF_QUEUED:
        value[CONF_MAX_RUNS] = 5

    if CONF_MAX_RUNS not in value:
        return value

    if value[CONF_MODE] not in [CONF_QUEUED, CONF_PARALLEL]:
        raise cv.Invalid(
            "The option 'max_runs' is only valid in 'queued' and 'parallel' mode.",
            path=[CONF_MAX_RUNS],
        )

    # Queued mode must have bounded queue (min 1), parallel mode can be unlimited (0)
    if value[CONF_MODE] == CONF_QUEUED and value[CONF_MAX_RUNS] < 1:
        raise cv.Invalid(
            "The option 'max_runs' must be at least 1 for queued mode.",
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
        array = False
        if type_.endswith("[]"):
            array = True
            type_ = type_[:-2]
        type_ = PARAMETER_TYPE_TRANSLATIONS.get(type_, type_)
        if array:
            type_ = f"std::vector<{type_}>"
        type_ = cg.esphome_ns.namespace(type_)
        template_args.append(type_)
        func_args.append((type_, name))
        script_arg_names.append(name)
    template = cg.TemplateArguments(*template_args)
    return template, func_args


def validate_parameter_name(value):
    value = cv.string(value)
    if value != CONF_ID:
        return value
    raise cv.Invalid(f"Script's parameter name cannot be {CONF_ID}")


ALLOWED_PARAM_TYPE_CHARSET = set("abcdefghijklmnopqrstuvwxyz0123456789_:*&[]<>")


def validate_parameter_type(value):
    value = cv.string_strict(value)
    if set(value.lower()) <= ALLOWED_PARAM_TYPE_CHARSET:
        return value
    raise cv.Invalid("Parameter type contains invalid characters")


def _fill_script_stop_ids(value: Any, script_id: str) -> Any:
    """Give `script.stop` actions without an id in a raw action tree the id `script_id`.

    Containers are copied only along changed paths so untouched YAML nodes keep their source locations.
    """
    if isinstance(value, list):
        result = [
            {CONF_SCRIPT_STOP: {CONF_ID: script_id}}
            if item == CONF_SCRIPT_STOP
            else _fill_script_stop_ids(item, script_id)
            for item in value
        ]
        if all(new is old for new, old in zip(result, value, strict=True)):
            return value
        return result
    if not isinstance(value, dict):
        return value
    result = value
    for key, item in value.items():
        if key == CONF_SCRIPT_STOP and (
            item is None or (isinstance(item, dict) and CONF_ID not in item)
        ):
            new_item = {**(item or {}), CONF_ID: script_id}
        else:
            new_item = _fill_script_stop_ids(item, script_id)
        if new_item is not item:
            if result is value:
                result = value.copy()
            result[key] = new_item
    return result


def _fill_script_stop_ids_in_script(item: Any) -> Any:
    if isinstance(item, dict) and isinstance(script_id := item.get(CONF_ID), str):
        return _fill_script_stop_ids(item, script_id)
    return item


def default_script_stop_ids(value: Any) -> Any:
    """Make `script.stop` with no id, used inside a script, stop that script."""
    if isinstance(value, list):
        return [_fill_script_stop_ids_in_script(item) for item in value]
    # A single script may be given as a mapping instead of a list
    return _fill_script_stop_ids_in_script(value)


CONFIG_SCHEMA = cv.All(
    default_script_stop_ids,
    automation.validate_automation(
        {
            # Don't declare id as cv.declare_id yet, because the ID type
            # depends on the mode. Will be checked later with assign_declare_id
            cv.Required(CONF_ID): cv.string_strict,
            cv.Optional(CONF_MODE, default=CONF_SINGLE): cv.one_of(
                *SCRIPT_MODES, lower=True
            ),
            cv.Optional(CONF_MAX_RUNS): cv.int_range(min=0, max=100),
            cv.Optional(CONF_PARAMETERS, default={}): cv.Schema(
                {
                    validate_parameter_name: validate_parameter_type,
                }
            ),
            cv.Optional(CONF_ON_STOP): automation.validate_automation(single=True),
        },
        extra_validators=cv.All(check_max_runs, assign_declare_id),
    ),
)


async def to_code(config):
    # Register all variables first, so that scripts can use other scripts
    pending = _data().pending
    triggers = []
    for conf in config:
        template, func_args = parameters_to_template(conf[CONF_PARAMETERS])
        trigger, pending[conf[CONF_ID].id] = _new_pvariable(conf[CONF_ID], template)
        # Add a human-readable name to the script
        cg.add(trigger.set_name(cg.LogStringLiteral(conf[CONF_ID].id)))

        if CONF_MAX_RUNS in conf:
            cg.add(trigger.set_max_runs(conf[CONF_MAX_RUNS]))

        if conf[CONF_MODE] == CONF_QUEUED:
            await cg.register_component(trigger, conf)

        triggers.append((trigger, func_args, conf))

    if any(CONF_ON_STOP in conf for conf in config):
        cg.add_define("USE_SCRIPT_ON_STOP")
    for trigger, func_args, conf in triggers:
        fn = await automation.build_automation_or_run_function(
            trigger, func_args, conf, conf[CONF_ID]
        )
        if fn is None:
            del pending[conf[CONF_ID].id]
        else:
            _use_function(trigger, conf[CONF_MODE], func_args, fn)
        if (stop_conf := conf.get(CONF_ON_STOP)) is not None:
            await automation.build_automation(trigger.get_stop_trigger(), [], stop_conf)


@automation.register_action(
    "script.execute",
    automation.ApplyAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(Script),
            cv.Optional(validate_parameter_name): cv.templatable(cv.valid),
        },
    ),
    synchronous=True,
)
async def script_execute_action_to_code(config, action_id, template_arg, args):
    def convert(type: str):
        def converter(value):
            if type == "std::string":
                return value
            if type == "bool":
                return cg.RawExpression(str(value).lower())
            if isinstance(value, (list, tuple)):
                return cg.ArrayInitializer(*value)
            return cg.RawExpression(str(value))

        return converter

    async def get_ordered_args(config, script_params):
        config_args = config.copy()
        config_args.pop(CONF_ID)

        # match script_args to the formal parameter order
        script_args = []
        for type, name in script_params:
            if name not in config_args:
                raise EsphomeError(
                    f"Missing parameter: '{name}' in script.execute {config[CONF_ID]}"
                )
            arg = await cg.templatable(
                config_args[name], args, type, convert(str(type))
            )
            script_args.append(arg)
        return script_args

    script = get_script(config[CONF_ID])
    params = script.get(CONF_PARAMETERS, [])
    _, script_params = parameters_to_template(params)
    script_args = await get_ordered_args(config, script_params)

    paren = await cg.get_variable(config[CONF_ID])
    # Separate statements, because the order of evaluation inside one call is not defined
    statements = [
        f"auto esphome__arg_{index} = {automation.immediate_lambda_call(arg)};"
        for index, arg in enumerate(script_args)
    ]
    names = ", ".join(
        f"std::move(esphome__arg_{index})" for index in range(len(script_args))
    )
    statements.append(f"{automation.parent_ref(paren)}->execute({names});")
    return automation.apply_statements_action(action_id, template_arg, args, statements)


def _require_script_stop_id(config: ConfigType) -> ConfigType:
    # Inside a script the id has already been filled in by default_script_stop_ids
    if CONF_ID not in config:
        raise cv.Invalid(
            "'script.stop' without an id can only be used inside a script",
            path=[CONF_ID],
        )
    return config


automation.register_apply_action(
    CONF_SCRIPT_STOP,
    maybe_simple_id({cv.Optional(CONF_ID): cv.use_id(Script)}, _require_script_stop_id),
    automation.ApplyCall("stop()"),
)


@automation.register_action(
    "script.wait",
    ScriptWaitAction,
    maybe_simple_id({cv.Required(CONF_ID): cv.use_id(Script)}),
    synchronous=False,
)
async def script_wait_action_to_code(config, action_id, template_arg, args):
    full_id, paren = await cg.get_variable_with_full_id(config[CONF_ID])
    template_arg = cg.TemplateArguments(full_id.type, *template_arg)
    var, statements = _new_pvariable(action_id, template_arg, paren)
    # Named again if the script turns out to run a function
    if (pending := _data().pending.get(full_id.id)) is not None:
        pending.extend(statements)
    await cg.register_component(var, {})
    return var


automation.register_apply_condition(
    "script.is_running",
    automation.maybe_simple_id({cv.Required(CONF_ID): cv.use_id(Script)}),
    "is_running()",
)

from collections.abc import Callable
from dataclasses import dataclass, field
from typing import Any

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components.const import CONF_ON_STOP
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE, CONF_PARAMETERS, CONF_RESTART
from esphome.core import CORE, EsphomeError
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
DOMAIN = "script"
script_ns = cg.esphome_ns.namespace("script")
Script = script_ns.class_("Script")
AutomationScript = script_ns.class_(
    "AutomationScript", Script, automation.Trigger.template()
)
FunctionBody = script_ns.class_("FunctionBody", Script)
FunctionScript = script_ns.class_("FunctionScript", Script)
FunctionPointerScript = script_ns.class_("FunctionPointerScript", Script)
ScriptWaitAction = script_ns.class_("ScriptWaitAction", automation.Action, cg.Component)

CONF_SINGLE = "single"
CONF_QUEUED = "queued"
CONF_PARALLEL = "parallel"
CONF_MAX_RUNS = "max_runs"
CONF_SCRIPT_STOP = "script.stop"


def _mode(
    name: str, *parents: cg.MockObjClass
) -> tuple[cg.MockObjClass, cg.MockObjClass]:
    """The class of a mode with an Automation, and its template on the class that runs."""
    return (
        script_ns.class_(f"{name}Script", AutomationScript, *parents),
        script_ns.class_(f"{name}Mode", Script, *parents),
    )


MODES = {
    CONF_SINGLE: _mode("Single"),
    CONF_RESTART: _mode("Restart"),
    CONF_QUEUED: _mode("Queueing", cg.Component),
    CONF_PARALLEL: _mode("Parallel"),
}
SCRIPT_MODES = {mode: classes[0] for mode, classes in MODES.items()}


@dataclass
class ScriptData:
    # By script id: the class script.wait is templated on, or the script.wait actions to
    # create once the script's class is known
    waits: dict[str, cg.MockObj | list[Callable[[cg.MockObj], None]]] = field(
        default_factory=dict
    )


def _data() -> ScriptData:
    return CORE.data.setdefault(DOMAIN, ScriptData())


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
    if value == CONF_ID:
        raise cv.Invalid(f"Script's parameter name cannot be {CONF_ID}")
    return value


def validate_parameter_names(value: Any) -> Any:
    """Reject reserved parameter names here: as dict keys of a schema they would only be
    reported as "extra keys not allowed"."""
    if isinstance(value, dict):
        for name in value:
            # Generated C++ code names its own variables like this
            if str(name).startswith("esphome__"):
                raise cv.Invalid(
                    "Names starting with 'esphome__' are reserved for generated code",
                    path=[name],
                )
    return value


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
            cv.Optional(CONF_PARAMETERS, default={}): cv.All(
                validate_parameter_names,
                cv.Schema({validate_parameter_name: validate_parameter_type}),
            ),
            cv.Optional(CONF_ON_STOP): automation.validate_automation(single=True),
        },
        extra_validators=cv.All(check_max_runs, assign_declare_id),
    ),
)


async def to_code(config):
    # Register all variables first, so that scripts can use other scripts. Each is created
    # once its actions are built, which decides its class, in the place it was declared.
    waits = _data().waits
    scripts = []
    for conf in config:
        template, func_args = parameters_to_template(conf[CONF_PARAMETERS])
        id_ = conf[CONF_ID].copy()
        id_.type = id_.type.template(template)
        late = cg.LatePvariable(id_)
        waits[id_.id] = []
        script = late.var
        # Add a human-readable name to the script
        cg.add(script.set_name(cg.LogStringLiteral(id_.id)))

        if CONF_MAX_RUNS in conf:
            cg.add(script.set_max_runs(conf[CONF_MAX_RUNS]))

        if queued := conf[CONF_MODE] == CONF_QUEUED:
            await cg.register_component(script, conf)

        scripts.append((late, template, func_args, conf, queued))

    if any(CONF_ON_STOP in conf for conf in config):
        cg.add_define("USE_SCRIPT_ON_STOP")
    for late, template, func_args, conf, queued in scripts:
        # The class or its constructor names the function, so it is declared in the script's place
        fn = await automation.build_automation_or_run_function(
            late.var, func_args, conf, late.global_slot.add
        )
        script_class = wait_class = late.id.type
        ctor_args = []
        if fn is not None:
            wait_class = MODES[conf[CONF_MODE]][1].template(
                FunctionBody.template(template), *template
            )
            if queued:
                # One class for all of them: a per-script class would also copy the
                # Component vtable, which costs more flash than the pointer's RAM
                script_class = FunctionPointerScript.template(wait_class, *template)
                ctor_args = [fn]
            else:
                script_class = FunctionScript.template(wait_class, fn, *template)
        late.define(script_class, *ctor_args)
        for create_wait in waits[late.id.id]:
            create_wait(wait_class)
        waits[late.id.id] = wait_class
        if (stop_conf := conf.get(CONF_ON_STOP)) is not None:
            await automation.build_automation(
                late.var.get_stop_trigger(), [], stop_conf
            )


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
    script_waits = _data().waits[full_id.id]
    pending = isinstance(script_waits, list)
    # Created where it is built, once the class of the script it waits for is known
    id_ = action_id.copy()
    id_.type = id_.type.template(
        full_id.type if pending else script_waits, *template_arg
    )
    late = cg.LatePvariable(id_)

    def create(script_class: cg.MockObj) -> None:
        late.define(
            action_id.type, cg.TemplateArguments(script_class, *template_arg), paren
        )

    if pending:
        script_waits.append(create)
    else:
        create(script_waits)
    var = late.var
    await cg.register_component(var, {})
    return var


automation.register_apply_condition(
    "script.is_running",
    automation.maybe_simple_id({cv.Required(CONF_ID): cv.use_id(Script)}),
    "is_running()",
)

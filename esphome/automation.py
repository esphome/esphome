from collections.abc import Callable
from dataclasses import dataclass, field
import logging
import string
from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALL,
    CONF_ANY,
    CONF_AUTOMATION_ID,
    CONF_CONDITION,
    CONF_COUNT,
    CONF_ELSE,
    CONF_ID,
    CONF_THEN,
    CONF_TIME,
    CONF_TIMEOUT,
    CONF_TRIGGER_ID,
    CONF_TYPE_ID,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import CORE, ID, EsphomeError, Lambda
from esphome.cpp_generator import (
    FlashStringLiteral,
    LambdaExpression,
    MockObj,
    MockObjClass,
    TemplateArgsType,
    call_lambda,
)
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType, SafeExpType
from esphome.util import Registry


def maybe_simple_id(*validators):
    """Allow a raw ID to be specified in place of a config block.
    If the value that's being validated is a dictionary, it's passed as-is to the specified validators. Otherwise, it's
    wrapped in a dict that looks like ``{"id": <value>}``, and that dict is then handed off to the specified validators.
    """
    return maybe_conf(CONF_ID, *validators)


def maybe_conf(conf, *validators):
    """Allow a raw value to be specified in place of a config block.
    If the value that's being validated is a dictionary, it's passed as-is to the specified validators. Otherwise, it's
    wrapped in a dict that looks like ``{<conf>: <value>}``, and that dict is then handed off to the specified
    validators.
    (This is a general case of ``maybe_simple_id`` that allows the wrapping key to be something other than ``id``.)
    """
    validator = cv.All(*validators)

    @schema_extractor("maybe")
    def validate(value):
        if value == SCHEMA_EXTRACT:
            return (validator, conf)

        if isinstance(value, dict):
            return validator(value)
        with cv.remove_prepend_path([conf]):
            return validator({conf: value})

    validate.inner_schema = validator
    return validate


_LOGGER = logging.getLogger(__name__)


def register_action(
    name: str,
    action_type: MockObjClass,
    schema: cv.Schema,
    *,
    synchronous: bool | None = None,
):
    """Register an action type.

    All callers must pass ``synchronous`` explicitly.

    ``synchronous=True`` — the action never defers ``play_next_()`` to a
    later point (callback, timer, or ``loop()``).  Trigger arguments are
    only used during the initial call, so string args can use non-owning
    StringRef for zero-copy access.

    ``synchronous=False`` — the action defers ``play_next_()`` via a
    callback, timer, or ``Component::loop()``.  Trigger arguments must
    outlive the initial call, so string args use owning std::string to
    prevent dangling references.
    """
    if synchronous is None:
        _LOGGER.warning(
            "register_action('%s', ...) is missing the synchronous= parameter. "
            "Defaulting to synchronous=False (safe but prevents StringRef "
            "optimization). Check the C++ class: use synchronous=False if "
            "play_next_() is deferred to a callback, timer, or loop(); "
            "use synchronous=True if play_next_() always runs before the "
            "initial play/play_complex call returns",
            name,
        )
        synchronous = False
    return ACTION_REGISTRY.register(name, action_type, schema, synchronous=synchronous)


def register_condition(name: str, condition_type: MockObjClass, schema: cv.Schema):
    return CONDITION_REGISTRY.register(name, condition_type, schema)


async def _build_with_parent(
    config: ConfigType,
    automation_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(automation_id, template_arg, parent)


async def _build_without_parent(
    config: ConfigType,
    automation_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    return cg.new_Pvariable(automation_id, template_arg)


async def _build_parented(
    config: ConfigType,
    automation_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(automation_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


def register_simple_action(
    name: str,
    action_type: MockObjClass,
    schema: cv.Schema,
    *,
    synchronous: bool,
) -> None:
    """Register an action whose constructor takes the object named by ``config[CONF_ID]``.

    Use the ``register_action`` decorator instead when the builder must also set fields.
    """
    register_action(name, action_type, schema, synchronous=synchronous)(
        _build_with_parent
    )


def register_simple_condition(
    name: str, condition_type: MockObjClass, schema: cv.Schema
) -> None:
    """Condition counterpart of ``register_simple_action``."""
    register_condition(name, condition_type, schema)(_build_with_parent)


def register_bare_action(
    name: str,
    action_type: MockObjClass,
    schema: cv.Schema,
    *,
    synchronous: bool,
) -> None:
    """Register an action whose constructor takes no arguments."""
    register_action(name, action_type, schema, synchronous=synchronous)(
        _build_without_parent
    )


def register_bare_condition(
    name: str, condition_type: MockObjClass, schema: cv.Schema
) -> None:
    """Condition counterpart of ``register_bare_action``."""
    register_condition(name, condition_type, schema)(_build_without_parent)


def register_parented_action(
    name: str,
    action_type: MockObjClass,
    schema: cv.Schema,
    *,
    synchronous: bool,
) -> None:
    """Register an action deriving from ``Parented<T>``.

    The object is constructed without arguments and ``set_parent()`` receives the object
    named by ``config[CONF_ID]``.
    """
    register_action(name, action_type, schema, synchronous=synchronous)(_build_parented)


def register_parented_condition(
    name: str, condition_type: MockObjClass, schema: cv.Schema
) -> None:
    """Condition counterpart of ``register_parented_action``."""
    register_condition(name, condition_type, schema)(_build_parented)


Action = cg.esphome_ns.class_("Action")
Trigger = cg.esphome_ns.class_("Trigger")
ACTION_REGISTRY = Registry()
Condition = cg.esphome_ns.class_("Condition")
CONDITION_REGISTRY = Registry()
validate_action = cv.validate_registry_entry("action", ACTION_REGISTRY)
validate_action_list = cv.validate_registry("action", ACTION_REGISTRY)
validate_condition = cv.validate_registry_entry("condition", CONDITION_REGISTRY)
validate_condition_list = cv.validate_registry("condition", CONDITION_REGISTRY)

ApplyAction = cg.esphome_ns.class_("ApplyAction", Action)


def flash_string(config: ConfigType, value: str) -> str:
    """Default renderer for ``std::string`` constants; copies the literal out of flash on ESP8266."""
    if CORE.is_esp8266:
        return f"progmem_string({FlashStringLiteral(value)})"
    return str(cg.safe_exp(value))


@dataclass(frozen=True)
class ApplyCall:
    """One statement from config keys, e.g. ``"set_range({}, {})"`` with ``((CONF_LOW, cg.float_), ...)``.

    Each arg is ``(conf_key, type_)`` or ``(conf_key, type_, const_fn)``. A ``conf_key`` may be a
    path into nested sections. A plain ``str`` ``type_`` is raw C++ type text and may use
    ``{parent}``. ``const_fn(config, value)`` renders a constant's argument text; a lambda bypasses
    it. The statement is skipped when none of its keys is set, always emitted when it has no
    keys, and a partial set is a config error.
    """

    target: str
    args: tuple[tuple[Any, ...], ...] = ()

    def __post_init__(self) -> None:
        fields = [
            f for _, f, _, _ in string.Formatter().parse(self.target) if f is not None
        ]
        if any(fields):
            raise ValueError(
                f"apply target {self.target!r}: only bare {{}} placeholders"
            )
        if len(fields) != len(self.args):
            raise ValueError(
                f"apply target {self.target!r} has {len(fields)} "
                f"placeholder(s) for {len(self.args)} config key(s)"
            )
        if any(len(arg) not in (2, 3) for arg in self.args):
            raise ValueError(
                f"apply target {self.target!r}: each arg is (conf_key, type_[, const_fn])"
            )


@dataclass(frozen=True)
class ApplyField:
    """One config key forwarded as ``target(value)``, or as statement ``target`` when it has ``{}``.

    Double a literal brace in a template. ``conf_key`` may be a path into nested sections.
    ``type_`` may be a C++ type string using ``{parent}`` when the type is only known per
    instance. ``const_fn(config, value)`` renders a constant's argument text when ``cg.safe_exp``
    is not the right spelling (unit conversion belongs in the validator); a lambda bypasses it,
    so the target must also take a plain ``type_``. An absent key emits nothing.
    """

    conf_key: str | tuple[str, ...]
    target: str
    type_: SafeExpType
    const_fn: Callable[[ConfigType, Any], str] | None = None

    def call(self) -> ApplyCall:
        target = self.target if "{}" in self.target else f"{self.target}({{}})"
        return ApplyCall(target, ((self.conf_key, self.type_, self.const_fn),))


def _config_lookup(config: ConfigType, key: str | tuple[str, ...]) -> Any:
    if isinstance(key, str):
        return config.get(key)
    for part in key:
        if (config := config.get(part)) is None:
            return None
    return config


def _dict_schema(schema: Any) -> Any:
    """The dict-backed cv.Schema inside cv.All and maybe_* wrappers, or None; cv.Any is not inspected."""
    if isinstance(getattr(schema, "schema", None), dict):
        return schema
    if isinstance(schema, cv.All):
        inner = schema.validators
    else:
        inner = (
            getattr(schema, "inner_schema", None),
        )  # maybe_conf / maybe_simple_value
    for candidate in inner:
        if candidate is not None and (found := _dict_schema(candidate)) is not None:
            return found
    return None


def _check_key_in_schema(
    name: str, schema: Any, conf_key: str | tuple[str, ...]
) -> None:
    """Reject a key path the schema does not have; a typo would otherwise be a silent no-op.

    Only dict-backed schemas, also inside cv.All and maybe_* wrappers, can be checked.
    """
    for part in (conf_key,) if isinstance(conf_key, str) else conf_key:
        if (schema := _dict_schema(schema)) is None:
            return
        markers = {
            getattr(marker, "schema", marker): marker for marker in schema.schema
        }
        if part not in markers:
            raise ValueError(f"{name}: config key {part!r} is not in the schema")
        schema = schema.schema[markers[part]]


def register_apply_action(
    name: str,
    schema: cv.Schema,
    *fields: ApplyField | ApplyCall,
    call: str | None = None,
) -> None:
    """Register an action that only forwards config values to its parent, with no C++ class.

    Generates one stateless function for ``ApplyAction<Ts...>``: parent and constants are baked
    in, lambdas are called inline with the trigger args. With ``call`` every statement targets
    the call object ``auto apply_call = parent->call()``, and ``apply_call.perform()`` is appended.
    """
    statements_spec = [
        (
            c.target,
            [(arg[0], arg[1], arg[2] if len(arg) == 3 else None) for arg in c.args],
        )
        for c in (f if isinstance(f, ApplyCall) else f.call() for f in fields)
    ]
    for _, members in statements_spec:
        for conf_key, _, _ in members:
            _check_key_in_schema(name, schema, conf_key)

    async def builder(
        config: ConfigType,
        action_id: ID,
        template_arg: cg.TemplateArguments,
        args: TemplateArgsType,
    ) -> MockObj:
        # Global-scope qualified so a trigger arg named like the id cannot shadow it.
        parent = f"::{await cg.get_variable(config[CONF_ID])}"
        # Must match ApplyAction::ApplyFn exactly for the function pointer conversion.
        lambda_args = [
            (cg.RawExpression(f"const std::remove_cvref_t<{cg.safe_exp(t)}> &"), arg)
            for t, arg in args
        ]
        receiver = "apply_call." if call else f"{parent}->"
        statements: list[str] = []
        for target, members in statements_spec:
            values = [_config_lookup(config, key) for key, _, _ in members]
            if members and all(value is None for value in values):
                continue
            if any(value is None for value in values):
                keys = [key for key, _, _ in members]
                raise EsphomeError(f"{name}: {target!r} needs all of {keys}")
            exprs: list[str] = []
            for (_, type_, const_fn), value in zip(members, values, strict=True):
                if isinstance(value, Lambda):
                    if isinstance(type_, str):
                        type_ = cg.RawExpression(type_.format(parent=parent))
                    inner = await cg.process_lambda(
                        value, lambda_args, return_type=type_
                    )
                    exprs.append(str(call_lambda(inner)))
                elif const_fn is not None:
                    exprs.append(const_fn(config, value))
                elif type_ is cg.std_string:
                    exprs.append(flash_string(config, value))
                else:
                    exprs.append(str(cg.safe_exp(value)))
            statements.append(f"{receiver}{target.format(*exprs)};")
        if call:
            statements = [
                f"auto apply_call = {parent}->{call}();",
                *statements,
                "apply_call.perform();",
            ]
        apply_lambda = LambdaExpression(
            ["\n".join(statements)], lambda_args, capture="", return_type=cg.void
        )
        return cg.new_Pvariable(action_id, template_arg, apply_lambda)

    register_action(name, ApplyAction, schema, synchronous=True)(builder)


def validate_potentially_and_condition(value):
    if isinstance(value, list):
        with cv.remove_prepend_path(["and"]):
            return validate_condition({"and": value})
    return validate_condition(value)


def validate_potentially_or_condition(value):
    if isinstance(value, list):
        with cv.remove_prepend_path(["or"]):
            return validate_condition({"or": value})
    return validate_condition(value)


DelayAction = cg.esphome_ns.class_("DelayAction", Action)
LambdaAction = cg.esphome_ns.class_("LambdaAction", Action)
StatelessLambdaAction = cg.esphome_ns.class_("StatelessLambdaAction", Action)
IfAction = cg.esphome_ns.class_("IfAction", Action)
WhileAction = cg.esphome_ns.class_("WhileAction", Action)
RepeatAction = cg.esphome_ns.class_("RepeatAction", Action)
WaitUntilAction = cg.esphome_ns.class_("WaitUntilAction", Action, cg.Component)
UpdateComponentAction = cg.esphome_ns.class_("UpdateComponentAction", Action)
SuspendComponentAction = cg.esphome_ns.class_("SuspendComponentAction", Action)
ResumeComponentAction = cg.esphome_ns.class_("ResumeComponentAction", Action)
Automation = cg.esphome_ns.class_("Automation")
TriggerForwarder = cg.esphome_ns.class_("TriggerForwarder")
TriggerOnTrueForwarder = cg.esphome_ns.class_("TriggerOnTrueForwarder")
TriggerOnFalseForwarder = cg.esphome_ns.class_("TriggerOnFalseForwarder")

LambdaCondition = cg.esphome_ns.class_("LambdaCondition", Condition)
StatelessLambdaCondition = cg.esphome_ns.class_("StatelessLambdaCondition", Condition)
ForCondition = cg.esphome_ns.class_("ForCondition", Condition, cg.Component)


def new_lambda_pvariable(
    id_obj: ID,
    lambda_expr: LambdaExpression,
    stateless_class: MockObjClass,
    template_arg: cg.TemplateArguments | None = None,
) -> MockObj:
    """Create Pvariable for lambda, using stateless class if applicable.

    Combines ID selection and Pvariable creation in one call. For stateless
    lambdas (empty capture), uses function pointer instead of std::function.

    Args:
        id_obj: The ID object (action_id, condition_id, or filter_id)
        lambda_expr: The lambda expression object
        stateless_class: The stateless class to use for stateless lambdas
        template_arg: Optional template arguments (for actions/conditions)

    Returns:
        The created Pvariable
    """
    # For stateless lambdas, use function pointer instead of std::function
    if lambda_expr.capture == "":
        id_obj = id_obj.copy()
        id_obj.type = stateless_class

    if template_arg is not None:
        return cg.new_Pvariable(id_obj, template_arg, lambda_expr)
    return cg.new_Pvariable(id_obj, lambda_expr)


def validate_automation(extra_schema=None, extra_validators=None, single=False):
    if extra_schema is None:
        extra_schema = {}
    if isinstance(extra_schema, cv.Schema):
        extra_schema = extra_schema.schema
    schema = AUTOMATION_SCHEMA.extend(extra_schema)

    def validator_(value):
        if isinstance(value, list):
            # List of items, there are two possible options here, either a sequence of
            # actions (no then:) or a list of automations.
            try:
                # First try as a sequence of actions
                # If that succeeds, return immediately
                with cv.remove_prepend_path([CONF_THEN]):
                    return [schema({CONF_THEN: value})]
            except cv.Invalid as err:
                # Next try as a sequence of automations
                try:
                    return cv.Schema([schema])(value)
                except cv.Invalid as err2:
                    if "extra keys not allowed" in str(err2) and len(err2.path) == 2:
                        raise err from None
                    if "Unable to find action" in str(err):
                        raise err2 from None
                    raise cv.MultipleInvalid([err, err2]) from None
        elif isinstance(value, dict):
            if CONF_THEN in value:
                return [schema(value)]
            with cv.remove_prepend_path([CONF_THEN]):
                return [schema({CONF_THEN: value})]
        # This should only happen with invalid configs, but let's have a nice error message.
        return [schema(value)]

    @schema_extractor("automation")
    def validator(value):
        if value == SCHEMA_EXTRACT:
            return schema

        value = validator_(value)
        if extra_validators is not None:
            value = cv.Schema([extra_validators])(value)
        if single:
            if len(value) != 1:
                raise cv.Invalid("This trigger allows only a single automation")
            return value[0]
        return value

    return validator


AUTOMATION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(Trigger),
        cv.GenerateID(CONF_AUTOMATION_ID): cv.declare_id(Automation),
        cv.Required(CONF_THEN): validate_action_list,
    }
)

AndCondition = cg.esphome_ns.class_("AndCondition", Condition)
OrCondition = cg.esphome_ns.class_("OrCondition", Condition)
NotCondition = cg.esphome_ns.class_("NotCondition", Condition)
XorCondition = cg.esphome_ns.class_("XorCondition", Condition)


@register_condition("and", AndCondition, validate_condition_list)
async def and_condition_to_code(
    config: ConfigType,
    condition_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    conditions = await build_condition_list(config, template_arg, args)
    return cg.new_Pvariable(
        condition_id, cg.TemplateArguments(len(conditions), *template_arg), conditions
    )


@register_condition("or", OrCondition, validate_condition_list)
async def or_condition_to_code(
    config: ConfigType,
    condition_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    conditions = await build_condition_list(config, template_arg, args)
    return cg.new_Pvariable(
        condition_id, cg.TemplateArguments(len(conditions), *template_arg), conditions
    )


@register_condition("all", AndCondition, validate_condition_list)
async def all_condition_to_code(
    config: ConfigType,
    condition_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    conditions = await build_condition_list(config, template_arg, args)
    return cg.new_Pvariable(
        condition_id, cg.TemplateArguments(len(conditions), *template_arg), conditions
    )


@register_condition("any", OrCondition, validate_condition_list)
async def any_condition_to_code(
    config: ConfigType,
    condition_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    conditions = await build_condition_list(config, template_arg, args)
    return cg.new_Pvariable(
        condition_id, cg.TemplateArguments(len(conditions), *template_arg), conditions
    )


@register_condition("not", NotCondition, validate_potentially_and_condition)
async def not_condition_to_code(
    config: ConfigType,
    condition_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    condition = await build_condition(config, template_arg, args)
    return cg.new_Pvariable(condition_id, template_arg, condition)


@register_condition("xor", XorCondition, validate_condition_list)
async def xor_condition_to_code(
    config: ConfigType,
    condition_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    conditions = await build_condition_list(config, template_arg, args)
    return cg.new_Pvariable(
        condition_id, cg.TemplateArguments(len(conditions), *template_arg), conditions
    )


@register_condition("lambda", LambdaCondition, cv.returning_lambda)
async def lambda_condition_to_code(
    config: ConfigType,
    condition_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    lambda_ = await cg.process_lambda(config, args, return_type=bool)
    return new_lambda_pvariable(
        condition_id, lambda_, StatelessLambdaCondition, template_arg
    )


@register_condition(
    "for",
    ForCondition,
    cv.Schema(
        {
            cv.Required(CONF_TIME): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
            cv.Required(CONF_CONDITION): validate_potentially_and_condition,
        }
    ).extend(cv.COMPONENT_SCHEMA),
)
async def for_condition_to_code(
    config: ConfigType,
    condition_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    condition = await build_condition(
        config[CONF_CONDITION], cg.TemplateArguments(), []
    )
    var = cg.new_Pvariable(condition_id, template_arg, condition)
    await cg.register_component(var, config)
    templ = await cg.templatable(config[CONF_TIME], args, cg.uint32)
    cg.add(var.set_time(templ))
    return var


@register_condition(
    "component.is_idle",
    LambdaCondition,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(cg.Component),
        }
    ),
)
async def component_is_idle_condition_to_code(
    config: ConfigType,
    condition_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    comp = await cg.get_variable(config[CONF_ID])
    lambda_ = await cg.process_lambda(
        Lambda(f"return {comp}->is_idle();"), args, return_type=bool
    )
    return new_lambda_pvariable(
        condition_id, lambda_, StatelessLambdaCondition, template_arg
    )


@register_action(
    "delay",
    DelayAction,
    cv.templatable(cv.positive_time_period_milliseconds),
    synchronous=False,
)
async def delay_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    template_ = await cg.templatable(config, args, cg.uint32)
    cg.add(var.set_delay(template_))
    return var


@register_action(
    "if",
    IfAction,
    cv.All(
        {
            cv.Exclusive(
                CONF_CONDITION, CONF_CONDITION
            ): validate_potentially_and_condition,
            cv.Exclusive(CONF_ANY, CONF_CONDITION): validate_potentially_or_condition,
            cv.Exclusive(CONF_ALL, CONF_CONDITION): validate_potentially_and_condition,
            cv.Optional(CONF_THEN): validate_action_list,
            cv.Optional(CONF_ELSE): validate_action_list,
        },
        cv.has_at_least_one_key(CONF_THEN, CONF_ELSE),
        cv.has_at_least_one_key(CONF_CONDITION, CONF_ANY, CONF_ALL),
    ),
    synchronous=True,
)
async def if_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    has_else = CONF_ELSE in config
    # Prepend HasElse bool to template arguments: IfAction<HasElse, Ts...>
    if_template_arg = cg.TemplateArguments(has_else, *template_arg)
    cond_conf = next(el for el in config if el in (CONF_ANY, CONF_ALL, CONF_CONDITION))
    condition = await build_condition(config[cond_conf], template_arg, args)
    var = cg.new_Pvariable(action_id, if_template_arg, condition)
    if CONF_THEN in config:
        actions = await build_action_list(config[CONF_THEN], template_arg, args)
        cg.add(var.add_then(actions))
    if has_else:
        actions = await build_action_list(config[CONF_ELSE], template_arg, args)
        cg.add(var.add_else(actions))
    return var


@register_action(
    "while",
    WhileAction,
    cv.Schema(
        {
            cv.Required(CONF_CONDITION): validate_potentially_and_condition,
            cv.Required(CONF_THEN): validate_action_list,
        }
    ),
    synchronous=True,
)
async def while_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    condition = await build_condition(config[CONF_CONDITION], template_arg, args)
    var = cg.new_Pvariable(action_id, template_arg, condition)
    actions = await build_action_list(config[CONF_THEN], template_arg, args)
    cg.add(var.add_then(actions))
    return var


@register_action(
    "repeat",
    RepeatAction,
    cv.Schema(
        {
            cv.Required(CONF_COUNT): cv.templatable(cv.positive_not_null_int),
            cv.Required(CONF_THEN): validate_action_list,
        }
    ),
    synchronous=True,
)
async def repeat_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    count_template = await cg.templatable(config[CONF_COUNT], args, cg.uint32)
    cg.add(var.set_count(count_template))
    actions = await build_action_list(
        config[CONF_THEN],
        cg.TemplateArguments(cg.uint32, *template_arg.args),
        [(cg.uint32, "iteration"), *args],
    )
    cg.add(var.add_then(actions))
    return var


_validate_wait_until = cv.maybe_simple_value(
    {
        cv.Required(CONF_CONDITION): validate_potentially_and_condition,
        cv.Optional(CONF_TIMEOUT): cv.templatable(cv.positive_time_period_milliseconds),
    },
    key=CONF_CONDITION,
)


@register_action("wait_until", WaitUntilAction, _validate_wait_until, synchronous=False)
async def wait_until_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    condition = await build_condition(config[CONF_CONDITION], template_arg, args)
    var = cg.new_Pvariable(action_id, template_arg, condition)
    if CONF_TIMEOUT in config:
        template_ = await cg.templatable(config[CONF_TIMEOUT], args, cg.uint32)
        cg.add(var.set_timeout_value(template_))
    await cg.register_component(var, {})
    return var


# Lambda executes user C++ inline and returns — synchronous by execution model.
# User code could theoretically store the StringRef for deferred use, but StringRef
# is a view type and storing views beyond their scope is always unsafe regardless
# of this optimization.  Marking non-synchronous would disable StringRef for nearly
# all user services since most use lambda.
@register_action("lambda", LambdaAction, cv.lambda_, synchronous=True)
async def lambda_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    lambda_ = await cg.process_lambda(config, args, return_type=cg.void)
    return new_lambda_pvariable(action_id, lambda_, StatelessLambdaAction, template_arg)


register_simple_action(
    "component.update",
    UpdateComponentAction,
    maybe_simple_id({cv.Required(CONF_ID): cv.use_id(cg.PollingComponent)}),
    synchronous=True,
)


register_simple_action(
    "component.suspend",
    SuspendComponentAction,
    maybe_simple_id({cv.Required(CONF_ID): cv.use_id(cg.PollingComponent)}),
    synchronous=True,
)


@register_action(
    "component.resume",
    ResumeComponentAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(cg.PollingComponent),
            cv.Optional(CONF_UPDATE_INTERVAL): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
        }
    ),
    synchronous=True,
)
async def component_resume_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    comp = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, comp)
    if CONF_UPDATE_INTERVAL in config:
        template_ = await cg.templatable(config[CONF_UPDATE_INTERVAL], args, cg.uint32)
        cg.add(var.set_update_interval(template_))
    return var


async def build_action(
    full_config: ConfigType, template_arg: cg.TemplateArguments, args: TemplateArgsType
) -> MockObj:
    registry_entry, config = cg.extract_registry_entry_config(
        ACTION_REGISTRY, full_config
    )
    action_id = full_config[CONF_TYPE_ID]
    builder = registry_entry.coroutine_fun
    return await builder(config, action_id, template_arg, args)


async def build_action_list(
    config: list[ConfigType], templ: cg.TemplateArguments, arg_type: TemplateArgsType
) -> list[MockObj]:
    actions: list[MockObj] = []
    for conf in config:
        action = await build_action(conf, templ, arg_type)
        actions.append(action)
    return actions


async def build_condition(
    full_config: ConfigType, template_arg: cg.TemplateArguments, args: TemplateArgsType
) -> MockObj:
    registry_entry, config = cg.extract_registry_entry_config(
        CONDITION_REGISTRY, full_config
    )
    action_id = full_config[CONF_TYPE_ID]
    builder = registry_entry.coroutine_fun
    return await builder(config, action_id, template_arg, args)


async def build_condition_list(
    config: ConfigType, templ: cg.TemplateArguments, args: TemplateArgsType
) -> list[MockObj]:
    conditions: list[MockObj] = []
    for conf in config:
        condition = await build_condition(conf, templ, args)
        conditions.append(condition)
    return conditions


def has_non_synchronous_actions(actions: ConfigType) -> bool:
    """Check if a validated action list contains any non-synchronous actions.

    Non-synchronous actions (delay, wait_until, script.wait, etc.) store
    trigger args for later execution, making non-owning types like StringRef
    unsafe.
    """
    if isinstance(actions, list):
        return any(has_non_synchronous_actions(item) for item in actions)
    if isinstance(actions, dict):
        for key in actions:
            if key in ACTION_REGISTRY and not ACTION_REGISTRY[key].synchronous:
                return True
        return any(
            has_non_synchronous_actions(v)
            for v in actions.values()
            if isinstance(v, (list, dict))
        )
    return False


async def build_automation(
    trigger: MockObj, args: TemplateArgsType, config: ConfigType
) -> MockObj:
    arg_types = [arg[0] for arg in args]
    templ = cg.TemplateArguments(*arg_types)
    obj = cg.new_Pvariable(config[CONF_AUTOMATION_ID], templ, trigger)
    actions = await build_action_list(config[CONF_THEN], templ, args)
    cg.add(obj.add_actions(actions))
    return obj


async def build_callback_automation(
    parent: MockObj,
    callback_method: str,
    args: TemplateArgsType,
    config: ConfigType,
    forwarder: MockObj | MockObjClass | None = None,
) -> None:
    """Build an Automation and register it as a callback on the parent.

    Eliminates the need for a Trigger wrapper object by registering the
    automation's trigger() directly as a callback on the parent component.

    Uses template forwarder structs so the compiler deduplicates the operator()
    body across all call sites with the same signature. The forwarder must be
    pointer-sized (single Automation* field) to fit inline in Callback::ctx_
    and avoid heap allocation.

    :param parent: The component object (e.g., button, sensor).
    :param callback_method: Name of the callback method (e.g., "add_on_press_callback").
    :param args: Automation template args as list of (type, name) tuples.
    :param config: The automation config dict.
    :param forwarder: Optional forwarder type to use instead of the default
        TriggerForwarder<Ts...>. Pass any struct type whose aggregate init takes
        a single Automation pointer (e.g., TriggerOnTrueForwarder).
    """
    arg_types = [arg[0] for arg in args]
    templ = cg.TemplateArguments(*arg_types)
    obj = cg.new_Pvariable(config[CONF_AUTOMATION_ID], templ)
    actions = await build_action_list(config[CONF_THEN], templ, args)
    cg.add(obj.add_actions(actions))
    # Use template forwarder structs for deduplication. The compiler generates
    # one operator() per forwarder type; different automation pointers are just
    # data in the struct.
    if forwarder is None:
        forwarder = TriggerForwarder.template(templ)
    # RawExpression for aggregate init — both forwarder and obj are codegen
    # MockObjs (not user input), and there's no Expression type for positional
    # aggregate initialization (StructInitializer uses named fields).
    cg.add(getattr(parent, callback_method)(cg.RawExpression(f"{forwarder}{{{obj}}}")))


@dataclass(frozen=True, slots=True)
class CallbackAutomation:
    """A single callback automation entry for build_callback_automations."""

    conf_key: str
    callback_method: str
    args: TemplateArgsType = field(default_factory=list)
    forwarder: MockObj | MockObjClass | None = None


async def build_callback_automations(
    parent: MockObj,
    config: ConfigType,
    entries: tuple[CallbackAutomation, ...],
) -> None:
    """Build multiple callback automations from a tuple of entries.

    :param parent: The component object (e.g., button, sensor).
    :param config: The full component config dict.
    :param entries: Tuple of CallbackAutomation entries to process.
    """
    for entry in entries:
        for conf in config.get(entry.conf_key, []):
            await build_callback_automation(
                parent,
                entry.callback_method,
                entry.args,
                conf,
                forwarder=entry.forwarder,
            )

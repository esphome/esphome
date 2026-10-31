from collections.abc import Awaitable, Callable, Sequence
from contextvars import ContextVar
from dataclasses import dataclass, field
import logging
import math
import string
import struct
from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ABOVE,
    CONF_ALL,
    CONF_ANY,
    CONF_AUTOMATION_ID,
    CONF_BELOW,
    CONF_CONDITION,
    CONF_COUNT,
    CONF_ELSE,
    CONF_ID,
    CONF_IF,
    CONF_THEN,
    CONF_TIME,
    CONF_TIMEOUT,
    CONF_TRIGGER_ID,
    CONF_TYPE_ID,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import CORE, ID, EsphomeError, HexInt, Lambda
from esphome.cpp_generator import (
    CallExpression,
    Expression,
    LambdaExpression,
    MockObj,
    MockObjClass,
    TemplateArgsType,
    call_lambda,
)
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType, SafeExpType
from esphome.util import Registry

CONF_CHOOSE = "choose"
CONF_CHOICE_ID = "choice_id"


def progmem_bytes(name: str, data: bytes | list[int]) -> MockObj:
    """Shared PROGMEM table for constant bytes; equal payloads share one, empty is nullptr."""
    if not data:
        return cg.nullptr
    return cg.shared_progmem_array(
        name, cg.uint8, cg.ArrayInitializer(*(HexInt(x) for x in data))
    )


async def templatable_bytes(
    value: Any,
    args: TemplateArgsType,
    set_template: MockObj,
    set_static: MockObj,
    table_name: str,
) -> None:
    """Set a TemplatableBytes: a lambda via set_template, constant bytes via set_static."""
    if cg.is_template(value):
        fn = await cg.templatable(value, args, cg.std_vector.template(cg.uint8))
        cg.add(set_template(fn))
    else:
        cg.add(set_static(*_static_bytes(table_name, value)))


def _static_bytes(table_name: str, value: bytes | list[int]) -> tuple[MockObj, int]:
    """The shared PROGMEM table for a constant payload and its length."""
    if (size := len(value)) > 0xFFFF:
        raise EsphomeError(f"Byte payload is {size} bytes; the maximum is 65535")
    return progmem_bytes(table_name, value), size


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
ApplyRunAction = cg.esphome_ns.class_("ApplyRunAction", Action)
ApplyCondition = cg.esphome_ns.class_("ApplyCondition", Condition)


@dataclass(frozen=True)
class _InlineAction:
    """An action whose builder only produced statements, not yet emitted as an object."""

    id_: ID
    statements: list[str]
    # None for control flow, which always needs the stop checks of a merged run
    emit: Callable[[], MockObj] | None


@dataclass(frozen=True)
class _InlineCondition:
    """A condition whose builder only produced an expression, not yet emitted as an object."""

    expression: str
    emit: Callable[[], MockObj]


@dataclass
class _InlineCollector:
    """Takes the statements of the action being built, instead of it emitting an object."""

    action_id: ID
    # For errors, from _builder_name
    name: str
    action: _InlineAction | None = None


_INLINE_COLLECTOR: ContextVar[_InlineCollector | None] = ContextVar(
    "esphome_automation_inline_collector", default=None
)


class _CollectedAction(MockObj):
    """What a collected builder returns: it names no C++ object, so any use of it is an error."""

    __slots__ = ()

    def _misuse(self) -> EsphomeError:
        collector = _INLINE_COLLECTOR.get()
        name = "an action" if collector is None else collector.name
        return EsphomeError(
            f"The builder of {name} used the result of its apply builder. It must return "
            "that result unchanged, because a merged action has no C++ object"
        )

    def __getattr__(self, attr: str) -> MockObj:
        # Python looks up dunder methods itself and expects AttributeError
        if attr.startswith("__"):
            raise AttributeError(attr)
        raise self._misuse()

    def __call__(self, *args: SafeExpType) -> MockObj:
        raise self._misuse()

    def __str__(self) -> str:
        raise self._misuse()


_COLLECTED_ACTION = _CollectedAction("esphome__collected_action", "->")
_RUNNING_PARAM = "esphome__running"
# One statement of its own, so a function that nothing can stop can leave them all out
_STOP_CHECK = f"if (!{_RUNNING_PARAM})\n  return;"


def _is_collecting(id_: ID) -> bool:
    collector = _INLINE_COLLECTOR.get()
    return collector is not None and collector.action_id is id_


def _collect_action(
    id_: ID, statements: list[str], emit: Callable[[], MockObj] | None
) -> bool:
    """Record ``id_`` as inline if it is the action or condition being collected.

    Matching the id leaves the conditions and child actions a builder creates to emit normally.
    """
    if not _is_collecting(id_):
        return False
    collector = _INLINE_COLLECTOR.get()
    # Replacing it would silently lose the statements already collected
    if collector.action is not None:
        raise EsphomeError(f"The builder of {collector.name} built its action twice")
    collector.action = _InlineAction(id_, statements, emit)
    return True


def _collect_control_flow(action_id: ID, statements: list[str]) -> MockObj:
    _collect_action(action_id, statements, None)
    return _COLLECTED_ACTION


def _control_flow_builders(action_id: ID) -> tuple[Callable, Callable]:
    """How a control flow builder builds its condition and branches.

    Only while it is being collected can the result become statements, so only then are
    they left unemitted.
    """
    if _is_collecting(action_id):
        return _build_condition_or_inline, _build_action_plan
    return build_condition, build_action_list


def _plans_inline(*plans: list[MockObj | list[_InlineAction]] | None) -> bool:
    return all(
        not plan or (len(plan) == 1 and isinstance(plan[0], list)) for plan in plans
    )


def _run_statements(run: list[_InlineAction]) -> list[str]:
    statements: list[str] = []
    for index, action in enumerate(run):
        # Any action can stop this automation, and then the rest must not run
        if index > 0:
            statements.append(_STOP_CHECK)
        statements.extend(["{", *action.statements, "}"])
    return statements


def _plan_statements(plan: list[MockObj | list[_InlineAction]] | None) -> list[str]:
    return _run_statements(plan[0]) if plan else []


def _if_statements(
    condition: str, then: list[str], otherwise: list[str] | None
) -> list[str]:
    # The condition itself can stop the automation
    statements = [
        "{",
        f"const bool esphome__condition = {condition};",
        _STOP_CHECK,
        "if (esphome__condition) {",
        *then,
    ]
    if otherwise is not None:
        statements.extend(["} else {", *otherwise])
    return [*statements, "}", "}"]


def _condition_expression(
    condition: MockObj | _InlineCondition, args: TemplateArgsType
) -> str:
    if isinstance(condition, _InlineCondition):
        return f"({condition.expression})"
    names = ", ".join(name for _, name in args)
    return f"{parent_ref(condition)}->check({names})"


def _condition_object(condition: MockObj | _InlineCondition) -> MockObj:
    return condition.emit() if isinstance(condition, _InlineCondition) else condition


def flash_string(config: ConfigType, value: str) -> str:
    """Default renderer for ``std::string`` constants; copies the literal out of flash on ESP8266."""
    return str(cg.progmem_string(value))


def flash_compare(config: ConfigType, value: str) -> str:
    """Default renderer for ``std::string`` constants in conditions; compares in flash on ESP8266."""
    if CORE.is_esp8266:
        return f"::esphome::ProgmemStringRef{{PSTR({cg.safe_exp(value)}), {len(value.encode('utf-8'))}}}"
    return str(cg.safe_exp(value))


def _default_std_string(
    members: list[tuple[str, Any, Any]], renderer: Callable[[ConfigType, str], str]
) -> list[tuple[str, Any, Any]]:
    """Give ``cg.std_string`` members without a renderer the given default."""
    return [
        (key, t, fn or (renderer if t is cg.std_string else None))
        for key, t, fn in members
    ]


def literal_with_length(config: ConfigType, value: str) -> str:
    """Renderer for a ``(const char *, size_t)`` target: a plain literal plus its byte length.

    The target compares or copies the bytes in place, so it needs the RAM literal rather than
    the PROGMEM rendering on ESP8266, and the length saves a strlen.
    """
    return f"{cg.safe_exp(value)}, {len(value.encode('utf-8'))}"


def string_ref_literal(config: ConfigType, value: str) -> str:
    """Renderer for a ``StringRef`` comparison: a flash literal on ESP8266, else ``StringRef(literal, length)``."""
    if CORE.is_esp8266:
        return str(cg.FlashStringLiteral(value))
    return f"StringRef({literal_with_length(config, value)})"


def _names_parent(text: str) -> bool:
    return any(f == "parent" for _, f, _, _ in string.Formatter().parse(text))


@dataclass(frozen=True)
class ApplyCall:
    """One statement from config keys, e.g. ``"set_range({}, {})"`` with ``((CONF_LOW, cg.float_), ...)``.

    Each arg is ``(conf_key, type_)`` or ``(conf_key, type_, const_fn)``. A ``conf_key`` may be a
    path into nested sections. ``target`` and a plain ``str`` ``type_`` (raw C++ type text) may
    name the parent object as ``{parent}``. ``const_fn(config, value)`` renders a constant's argument text; a lambda or an
    id bypasses it. The statement is skipped when none of its keys is set, always emitted when it
    has no keys, and a partial set is a config error.
    """

    target: str
    args: tuple[tuple[Any, ...], ...] = ()

    def __post_init__(self) -> None:
        fields = [
            f for _, f, _, _ in string.Formatter().parse(self.target) if f is not None
        ]
        if any(f not in ("", "parent") for f in fields):
            raise ValueError(
                f"apply target {self.target!r}: only {{}} and {{parent}} placeholders"
            )
        if (count := fields.count("")) != len(self.args):
            raise ValueError(
                f"apply target {self.target!r} has {count} "
                f"placeholder(s) for {len(self.args)} config key(s)"
            )
        if any(len(arg) not in (2, 3) for arg in self.args):
            raise ValueError(
                f"apply target {self.target!r}: each arg is (conf_key, type_[, const_fn])"
            )

    @property
    def names_parent(self) -> bool:
        return _names_parent(self.target) or any(
            isinstance(arg[1], str) and _names_parent(arg[1]) for arg in self.args
        )

    @property
    def members(self) -> list[tuple[Any, Any, Any]]:
        """Each arg as ``(conf_key, type_, const_fn or None)``."""
        return [
            (arg[0], arg[1], arg[2] if len(arg) == 3 else None) for arg in self.args
        ]


@dataclass(frozen=True)
class ApplyField:
    """One config key forwarded as ``target(value)``, or as statement ``target`` when it has ``{}``.

    Double a literal brace in a template. ``conf_key`` may be a path into nested sections.
    ``type_`` may be a C++ type string using ``{parent}`` when the type is only known per
    instance. ``const_fn(config, value)`` renders a constant's argument text when ``cg.safe_exp``
    is not the right spelling, or the value depends on another entity's config, such as the unit
    a climate reports in; a lambda or an id bypasses it, so the target must also take a plain
    ``type_``. An absent key emits nothing.
    """

    conf_key: str | tuple[str, ...]
    target: str
    type_: SafeExpType
    const_fn: Callable[[ConfigType, Any], str] | None = None

    def call(self) -> ApplyCall:
        target = self.target if "{}" in self.target else f"{self.target}({{}})"
        return ApplyCall(target, ((self.conf_key, self.type_, self.const_fn),))


def _in_range_low(config: ConfigType, value: float) -> str:
    return "-INFINITY" if math.isnan(value) else str(cg.safe_exp(value))


def _in_range_high(config: ConfigType, value: float) -> str:
    # With no lower bound, an unset upper bound stays NaN so the check is false, as before.
    if math.isnan(value):
        return "NAN" if math.isnan(config[CONF_ABOVE]) else "INFINITY"
    return str(cg.safe_exp(value))


# ``above <= state <= below`` on the parent's float ``state``; register with ``IN_RANGE_DEFAULTS``.
IN_RANGE_CHECK = ApplyCall(
    "state >= {} && {parent}->state <= {}",
    ((CONF_ABOVE, cg.float_, _in_range_low), (CONF_BELOW, cg.float_, _in_range_high)),
)
IN_RANGE_DEFAULTS = {CONF_ABOVE: math.nan, CONF_BELOW: math.nan}


@dataclass(frozen=True)
class ApplyBytes:
    """A byte payload key forwarded as ``target``, whose ``{}`` stands for ``data, len``.

    A constant payload is a shared PROGMEM table passed as is; on ESP8266 it is first copied
    out, to the stack up to ``stack`` bytes and to the heap beyond, unless ``progmem_target``
    names a call that reads flash itself. A lambda's ``std::vector<uint8_t>`` is passed as
    its data and size. The template may name the parent as ``{parent}``. An absent key
    emits nothing.
    """

    conf_key: str | tuple[str, ...]
    target: str
    stack: int = 64
    progmem_target: str | None = None

    def __post_init__(self) -> None:
        for target in (self.target, self.progmem_target):
            if target is None:
                continue
            fields = [
                f for _, f, _, _ in string.Formatter().parse(target) if f is not None
            ]
            if fields.count("") != 1 or any(f not in ("", "parent") for f in fields):
                raise ValueError(f"apply bytes target {target!r} needs one {{}}")


def _config_lookup(config: ConfigType, key: str | tuple[str, ...]) -> Any:
    if isinstance(key, str):
        return config.get(key)
    for part in key:
        if (config := config.get(part)) is None:
            return None
    return config


def _dict_schema(schema: Any) -> Any:
    """The dict-backed cv.Schema inside cv.All and maybe_* wrappers, or None; cv.Any is not inspected."""
    if isinstance(schema, dict):
        return cv.Schema(schema)
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


def parent_ref(var: MockObj) -> MockObj:
    """``var`` named from global scope, so a trigger argument cannot shadow it.

    Also how a generated callback names its Automation.
    """
    return MockObj(f"::{var}", "->")


async def _apply_parent(config: ConfigType, id_key: str = CONF_ID) -> str:
    return str(parent_ref(await cg.get_variable(config[id_key])))


def _apply_lambda_args(args: TemplateArgsType) -> TemplateArgsType:
    # The generated function's parameters: a reference keeps its spelling, so a mutable
    # one stays mutable; a value is taken by const reference, so a std::string is never copied.
    return [
        (
            (
                t
                if str(cg.safe_exp(t)).rstrip().endswith("&")
                else cg.RawExpression(f"const std::remove_cvref_t<{cg.safe_exp(t)}> &")
            ),
            arg,
        )
        for t, arg in args
    ]


def _apply_function(
    id_: ID,
    return_type: SafeExpType,
    template_arg: cg.TemplateArguments,
    lambda_args: TemplateArgsType,
    statements: list[str],
) -> MockObj:
    """Emit the generated function and declare ``id_`` as the ``ApplyAction`` or
    ``ApplyCondition`` templated on it, so ``play()`` calls it directly."""

    def emit() -> MockObj:
        fn = apply_static_function(id_, return_type, lambda_args, statements)
        return cg.new_Pvariable(id_, cg.TemplateArguments(fn, *template_arg))

    return _COLLECTED_ACTION if _collect_action(id_, statements, emit) else emit()


def apply_statements_action(
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
    statements: list[str],
) -> MockObj:
    """Build an action from C++ statements, for a builder registered with ``ApplyAction``.

    Use it when the statements do not fit ``register_apply_action``. Generated names should
    start with ``esphome__`` so a trigger argument cannot shadow them.
    """
    return _apply_function(
        action_id, cg.void, template_arg, _apply_lambda_args(args), statements
    )


def apply_static_function(
    id_: ID,
    return_type: SafeExpType,
    params: TemplateArgsType,
    statements: list[str],
) -> MockObj:
    """Emit the static function generated for ``id_`` and return an expression naming it."""
    return cg.static_function(f"esphome__{id_.id}__fn", return_type, params, statements)


async def _render_values(
    name: str,
    target: str,
    members: list[tuple[Any, Any, Any]],
    values: list[Any],
    config: ConfigType,
    parent: str | None,
    lambda_args: TemplateArgsType,
    compare: bool = False,
) -> list[str]:
    """Render the argument text of one statement; every key must be present.

    ``compare``: an inlined lambda expression is parenthesized so it binds as a whole
    beside an operator.
    """
    if any(value is None for value in values):
        keys = [key for key, _, _ in members]
        raise EsphomeError(f"{name}: {target!r} needs all of {keys}")
    exprs: list[str] = []
    for (_, type_, const_fn), value in zip(members, values, strict=True):
        if isinstance(value, Lambda):
            if isinstance(type_, str):
                type_ = cg.RawExpression(type_.format(parent=parent))
            inner = await cg.process_lambda(value, lambda_args, return_type=type_)
            expr = call_lambda(inner)
            bare = compare and isinstance(expr, cg.RawExpression)
            exprs.append(f"({expr})" if bare else str(expr))
        elif isinstance(value, ID):
            # Qualified like the parent, so a trigger arg named like the id cannot shadow it.
            exprs.append(f"::{await cg.get_variable(value)}")
        elif const_fn is not None:
            exprs.append(const_fn(config, value))
        else:
            exprs.append(str(cg.safe_exp(value)))
    return exprs


def _apply_values(config: ConfigType, members: list[tuple[Any, Any, Any]]) -> list[Any]:
    return [_config_lookup(config, key) for key, _, _ in members]


def register_apply_action(
    name: str,
    schema: cv.Schema,
    *fields: ApplyField | ApplyCall | ApplyBytes,
    call: str | None = None,
    id_key: str = CONF_ID,
) -> None:
    """Register an action that only forwards config values to its parent, with no C++ class.

    See ``apply_action_builder``.
    """
    register_action(name, ApplyAction, schema, synchronous=True)(
        apply_action_builder(name, schema, *fields, call=call, id_key=id_key)
    )


def apply_action_builder(
    name: str,
    schema: cv.Schema,
    *fields: ApplyField | ApplyCall | ApplyBytes,
    call: str | None = None,
    id_key: str = CONF_ID,
) -> Callable[..., Awaitable[MockObj]]:
    """The builder ``register_apply_action`` registers, for a caller that wraps it.

    Generates one static function with the parent (read from ``id_key``) and constants baked
    in, lambdas called inline with the trigger args, and an ``ApplyAction`` templated on it.
    A constant that is an id (``cv.use_id`` under ``cv.templatable``) is the object it names.
    With ``call`` every statement targets the call object ``auto apply_call = parent->call()``,
    and ``apply_call.perform()`` is appended.

    A wrapper may do other work first, but must return the builder's result unchanged and not
    use it: when the action merges with its neighbors, the result names no C++ object.
    """
    # An action stores the value, so a std::string constant stays in flash on ESP8266.
    _check_key_in_schema(name, schema, id_key)
    statements_spec: list[ApplyBytes | tuple[str, list[tuple[Any, Any, Any]]]] = []
    for f in fields:
        if isinstance(f, ApplyBytes):
            _check_key_in_schema(name, schema, f.conf_key)
            statements_spec.append(f)
            continue
        c = f if isinstance(f, ApplyCall) else f.call()
        members = _default_std_string(c.members, flash_string)
        for conf_key, _, _ in members:
            _check_key_in_schema(name, schema, conf_key)
        statements_spec.append((c.target, members))
    # Named like the generated function, so it cannot clash with an SDK symbol
    table_name = f"esphome__{name.replace('.', '_')}__bytes"

    async def builder(
        config: ConfigType,
        action_id: ID,
        template_arg: cg.TemplateArguments,
        args: TemplateArgsType,
    ) -> MockObj:
        parent = await _apply_parent(config, id_key)
        lambda_args = _apply_lambda_args(args)
        receiver = "apply_call." if call else f"{parent}->"
        statements: list[str] = []
        for spec in statements_spec:
            if isinstance(spec, ApplyBytes):
                statements.extend(
                    await _render_bytes(
                        spec, config, receiver, parent, args, table_name
                    )
                )
                continue
            target, members = spec
            values = _apply_values(config, members)
            if not _apply_call_active(members, values):
                continue
            exprs = await _render_values(
                name, target, members, values, config, parent, lambda_args
            )
            statements.append(f"{receiver}{target.format(*exprs, parent=parent)};")
        if call:
            statements = [
                f"auto apply_call = {parent}->{call}();",
                *statements,
                "apply_call.perform();",
            ]
        return _apply_function(
            action_id, cg.void, template_arg, lambda_args, statements
        )

    return builder


async def _render_bytes(
    spec: ApplyBytes,
    config: ConfigType,
    receiver: str,
    parent: str,
    args: TemplateArgsType,
    table_name: str,
) -> list[str]:
    """The statements for one ``ApplyBytes``: what ``TemplatableBytes::visit`` does, inlined.

    The payload lambda keeps the automation's own argument types, as ``TemplatableBytes``
    gave it, and is always called, so a body that modifies an argument still compiles.
    """
    value = _config_lookup(config, spec.conf_key)
    if value is None:
        return []

    # The temporary carries the generated-name prefix so a trigger argument cannot shadow it
    def send(target: str, data: str) -> str:
        return f"{receiver}{target.format(data, parent=parent)};"

    if isinstance(value, Lambda):
        inner = await cg.process_lambda(
            value, args, return_type=cg.std_vector.template(cg.uint8)
        )
        # Never reduced into the outer function, whose own parameters are const
        call = CallExpression(inner, *[cg.MockObj(name) for _, name in args])
        return [
            "{",
            f"  const std::vector<uint8_t> esphome__bytes = {call};",
            f"  {send(spec.target, 'esphome__bytes.data(), esphome__bytes.size()')}",
            "}",
        ]
    table, size = _static_bytes(table_name, value)
    # Qualified so a trigger argument cannot shadow a reused table; an empty payload is nullptr
    table_args = f"::{table}, {size}" if size else f"{table}, {size}"
    if CORE.is_esp8266 and spec.progmem_target is not None:
        return [send(spec.progmem_target, table_args)]
    if not CORE.is_esp8266:
        return [send(spec.target, table_args)]
    # A PROGMEM table must be read with aligned loads on ESP8266, so copy it out first;
    # the length is known here, so the copy is an exact array, on the heap past ``stack``.
    # An empty payload still gets a one byte array: the raw lwIP socket rejects a null buffer.
    if size <= spec.stack:
        buffer, data = f"uint8_t esphome__bytes[{max(size, 1)}];", "esphome__bytes"
    else:
        buffer, data = (
            f"auto esphome__bytes = std::make_unique<uint8_t[]>({size});",
            "esphome__bytes.get()",
        )
    statements = ["{", f"  {buffer}"]
    if size:
        statements.append(f"  esphome::progmem_memcpy({data}, {table_args});")
    return [*statements, f"  {send(spec.target, f'{data}, {size}')}", "}"]


def _apply_call_active(members: list[tuple[Any, Any, Any]], values: list[Any]) -> bool:
    """An ``ApplyCall`` is emitted unless it has keys and none of them is set."""
    return not members or any(value is not None for value in values)


async def _render_check(
    name: str,
    target: str,
    members: list[tuple[Any, Any, Any]],
    values: list[Any],
    config: ConfigType,
    parent: str | None,
    lambda_args: TemplateArgsType,
) -> str:
    """Render a boolean check with ``values`` compared against config."""
    exprs = await _render_values(
        name, target, members, values, config, parent, lambda_args, compare=True
    )
    return target.format(*exprs, parent=parent)


def register_apply_condition(
    name: str,
    schema: cv.Schema,
    check: str | ApplyCall,
    id_key: str = CONF_ID,
    defaults: ConfigType | None = None,
) -> None:
    """Register a condition that is one expression on its parent, with no C++ class.

    ``check`` is applied to the parent: ``"is_playing()"`` becomes ``parent->is_playing()``; an
    ``ApplyCall`` such as ``ApplyCall("state == {}", ((CONF_STATE, cg.bool_),))`` compares
    against config values, all of which must be present. Write ``== false`` to negate.
    A ``cg.std_string`` constant stays in flash on ESP8266 and supports only ``==`` or ``!=``
    against a ``std::string`` or ``StringRef`` member; other string types are plain literals.
    ``defaults`` fills absent keys at code generation only, so the validated config is unchanged.
    Generates one static predicate and an ``ApplyCondition`` templated on it.
    """
    call = check if isinstance(check, ApplyCall) else ApplyCall(check)
    members = _default_std_string(call.members, flash_compare)
    _check_key_in_schema(name, schema, id_key)
    for conf_key, _, _ in members:
        _check_key_in_schema(name, schema, conf_key)

    async def builder(
        config: ConfigType,
        condition_id: ID,
        template_arg: cg.TemplateArguments,
        args: TemplateArgsType,
    ) -> MockObj:
        if defaults:
            config = {**defaults, **config}
        parent = await _apply_parent(config, id_key)
        lambda_args = _apply_lambda_args(args)
        values = _apply_values(config, members)
        check = await _render_check(
            name, call.target, members, values, config, parent, lambda_args
        )
        return _apply_function(
            condition_id,
            cg.bool_,
            template_arg,
            lambda_args,
            [f"return {parent}->{check};"],
        )

    register_condition(name, ApplyCondition, schema)(builder)


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
    # Not ``in``: comparing two MockObj values gives a truthy MockObj
    if (
        lambda_expr.capture == ""
        and (
            stateless_class is StatelessLambdaAction
            or stateless_class is StatelessLambdaCondition
        )
        and _collect_action(
            id_obj,
            [f"{immediate_lambda_call(lambda_expr)};"],
            lambda: new_lambda_pvariable(
                id_obj, lambda_expr, stateless_class, template_arg
            ),
        )
    ):
        return _COLLECTED_ACTION

    # For stateless lambdas, use function pointer instead of std::function
    if lambda_expr.capture == "":
        id_obj = id_obj.copy()
        id_obj.type = stateless_class

    if template_arg is not None:
        return cg.new_Pvariable(id_obj, template_arg, lambda_expr)
    return cg.new_Pvariable(id_obj, lambda_expr)


def immediate_lambda_call(lambda_expr: LambdaExpression) -> CallExpression:
    """Call a lambda on the spot with its own parameter names; a ``return`` ends only the lambda."""
    names = [MockObj(param.id) for param in lambda_expr.parameters.parameters]
    return CallExpression(lambda_expr, *names)


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
@register_condition("all", AndCondition, validate_condition_list)
@register_condition("or", OrCondition, validate_condition_list)
@register_condition("any", OrCondition, validate_condition_list)
@register_condition("xor", XorCondition, validate_condition_list)
async def condition_group_to_code(
    config: ConfigType,
    condition_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    # Xor of several stays an object, which evaluates every condition in order; C++ does not
    # define the order of the operands of ``+``.
    if (
        config
        and _is_collecting(condition_id)
        and (len(config) == 1 or condition_id.type is not XorCondition)
    ):
        items = [
            await _build_condition_or_inline(c, template_arg, args) for c in config
        ]

        def emit() -> MockObj:
            return _new_condition_group(
                condition_id, template_arg, [_condition_object(i) for i in items]
            )

        operator = " || " if condition_id.type is OrCondition else " && "
        check = operator.join(_condition_expression(item, args) for item in items)
        _collect_action(condition_id, [f"return {check};"], emit)
        return _COLLECTED_ACTION
    conditions = await build_condition_list(config, template_arg, args)
    return _new_condition_group(condition_id, template_arg, conditions)


def _new_condition_group(
    condition_id: ID, template_arg: cg.TemplateArguments, conditions: list[MockObj]
) -> MockObj:
    # A group of one is that condition for and, or and xor; skip the wrapper.
    if len(conditions) == 1:
        return conditions[0]
    return cg.new_Pvariable(
        condition_id, cg.TemplateArguments(len(conditions), *template_arg), *conditions
    )


@register_condition("not", NotCondition, validate_potentially_and_condition)
async def not_condition_to_code(
    config: ConfigType,
    condition_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    if _is_collecting(condition_id):
        item = await _build_condition_or_inline(config, template_arg, args)
        _collect_action(
            condition_id,
            [f"return !{_condition_expression(item, args)};"],
            lambda: cg.new_Pvariable(
                condition_id, template_arg, _condition_object(item)
            ),
        )
        return _COLLECTED_ACTION
    condition = await build_condition(config, template_arg, args)
    return cg.new_Pvariable(condition_id, template_arg, condition)


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


register_apply_condition(
    "component.is_idle",
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(cg.Component),
        }
    ),
    "is_idle()",
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


def _wrap_bare_if_condition(value: Any) -> Any:
    """Allow a single condition directly under `if:` in place of `condition: {...}`."""
    if not isinstance(value, dict):
        return value
    # `any` and `all` are also registered conditions, but here they are the if action's own keys
    bare = [
        key
        for key in value
        if key in CONDITION_REGISTRY and key not in (CONF_ANY, CONF_ALL)
    ]
    if not bare:
        return value
    if len(bare) > 1:
        raise cv.Invalid(
            f"Only one condition may be given directly under 'if', found {', '.join(bare)}. "
            f"Use '{CONF_CONDITION}', '{CONF_ALL}' or '{CONF_ANY}' to combine conditions"
        )
    if other := [k for k in (CONF_CONDITION, CONF_ANY, CONF_ALL) if k in value]:
        raise cv.Invalid(
            f"'{bare[0]}' given directly under 'if' cannot be used with '{other[0]}'. "
            f"Move it into '{other[0]}'"
        )
    key = bare[0]
    result = {k: v for k, v in value.items() if k != key}
    result[CONF_CONDITION] = {key: value[key]}
    return result


@register_action(
    "if",
    IfAction,
    cv.All(
        _wrap_bare_if_condition,
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
    build_cond, build_list = _control_flow_builders(action_id)
    condition = await build_cond(config[cond_conf], template_arg, args)
    then_actions = else_actions = None
    if CONF_THEN in config:
        then_actions = await build_list(config[CONF_THEN], template_arg, args)
    if has_else:
        else_actions = await build_list(config[CONF_ELSE], template_arg, args)
    if _is_collecting(action_id) and _plans_inline(then_actions, else_actions):
        return _collect_control_flow(
            action_id,
            _if_statements(
                _condition_expression(condition, args),
                _plan_statements(then_actions),
                _plan_statements(else_actions) if has_else else None,
            ),
        )
    var = cg.new_Pvariable(action_id, if_template_arg, _condition_object(condition))
    _add_if_branches(
        var,
        None
        if then_actions is None
        else _emit_action_plan(then_actions, template_arg, args),
        None
        if else_actions is None
        else _emit_action_plan(else_actions, template_arg, args),
    )
    return var


def _add_if_branches(
    var: MockObj,
    then_actions: list[MockObj] | None,
    else_actions: list[MockObj] | None,
) -> None:
    """Add the branches of an IfAction one action at a time; None leaves a branch out."""
    if then_actions is not None:
        for action in then_actions:
            cg.add(var.add_then_action(action))
        cg.add(var.finish_then())
    if else_actions is not None:
        for action in else_actions:
            cg.add(var.add_else_action(action))
        cg.add(var.finish_else())


def _validate_choice(choice: ConfigType) -> ConfigType:
    """A choice is either `if` with `then`, or `else` on its own."""
    if CONF_ELSE in choice:
        for key in (CONF_IF, CONF_THEN):
            if key in choice:
                raise cv.Invalid(f"'{key}' cannot be used with 'else'", path=[key])
        return choice
    for key in (CONF_IF, CONF_THEN):
        if key not in choice:
            raise cv.Invalid(f"'{key}' is required", path=[key])
    return choice


def _validate_choose_else(choices: list[ConfigType]) -> list[ConfigType]:
    """`else` may only be the last choice, and not the only one."""
    for index, choice in enumerate(choices[:-1]):
        if CONF_ELSE in choice:
            raise cv.Invalid(
                "Only the last choice may be 'else'", path=[index, CONF_ELSE]
            )
    if len(choices) == 1 and CONF_ELSE in choices[0]:
        raise cv.Invalid(
            "At least one choice must have 'if' and 'then'", path=[0, CONF_ELSE]
        )
    return choices


@register_action(
    CONF_CHOOSE,
    IfAction,
    cv.All(
        cv.ensure_list(
            cv.All(
                cv.Schema(
                    {
                        cv.GenerateID(CONF_CHOICE_ID): cv.declare_id(IfAction),
                        cv.Optional(CONF_IF): validate_potentially_and_condition,
                        cv.Optional(CONF_THEN): validate_action_list,
                        cv.Optional(CONF_ELSE): validate_action_list,
                    }
                ),
                _validate_choice,
            )
        ),
        cv.Length(min=1),
        _validate_choose_else,
    ),
    synchronous=True,
)
async def choose_action_to_code(
    config: list[ConfigType],
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    # Built as a chain of if/else actions: each choice's else branch holds the next choice,
    # and the last choice's else branch holds the `else` actions, if any.
    build_cond, build_list = _control_flow_builders(action_id)
    else_actions: list[MockObj] | None = None
    if CONF_ELSE in config[-1]:
        else_actions = await build_list(config[-1][CONF_ELSE], template_arg, args)
        config = config[:-1]
    built = [
        (
            await build_cond(choice[CONF_IF], template_arg, args),
            await build_list(choice[CONF_THEN], template_arg, args),
        )
        for choice in config
    ]
    # A lambda can reach a choice by a manual id, so those stay objects
    if (
        _is_collecting(action_id)
        and not any(choice[CONF_CHOICE_ID].is_manual for choice in config)
        and _plans_inline(else_actions, *(then for _, then in built))
    ):
        statements = None if else_actions is None else _plan_statements(else_actions)
        for condition, then_actions in reversed(built):
            statements = _if_statements(
                _condition_expression(condition, args),
                _plan_statements(then_actions),
                statements,
            )
        return _collect_control_flow(action_id, statements)
    if else_actions is not None:
        else_actions = _emit_action_plan(else_actions, template_arg, args)
    var: MockObj | None = None
    for index in reversed(range(len(config))):
        choice = config[index]
        condition, then_actions = built[index]
        if_template_arg = cg.TemplateArguments(else_actions is not None, *template_arg)
        choice_id = action_id if index == 0 else choice[CONF_CHOICE_ID]
        var = cg.new_Pvariable(choice_id, if_template_arg, _condition_object(condition))
        _add_if_branches(
            var, _emit_action_plan(then_actions, template_arg, args), else_actions
        )
        else_actions = [var]
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
    build_cond, build_list = _control_flow_builders(action_id)
    condition = await build_cond(config[CONF_CONDITION], template_arg, args)
    actions = await build_list(config[CONF_THEN], template_arg, args)
    # An empty body exits the loop at once, unlike an empty C++ loop body
    if _is_collecting(action_id) and actions and _plans_inline(actions):
        return _collect_control_flow(
            action_id,
            [
                "while (true) {",
                f"const bool esphome__condition = {_condition_expression(condition, args)};",
                _STOP_CHECK,
                "if (!esphome__condition)\n  break;",
                *_plan_statements(actions),
                _STOP_CHECK,
                "}",
            ],
        )
    var = cg.new_Pvariable(action_id, template_arg, _condition_object(condition))
    for action in _emit_action_plan(actions, template_arg, args):
        cg.add(var.add_then_action(action))
    cg.add(var.finish_then())
    return var


def _shadow_iteration(args: TemplateArgsType) -> TemplateArgsType:
    """Rename an inherited `iteration` so the inner repeat's own counter can shadow it."""
    names = {name for _, name in args}
    renamed: TemplateArgsType = []
    for type_, name in args:
        if name == "iteration":
            depth = 1
            while f"iteration_{depth}" in names:
                depth += 1
            name = f"iteration_{depth}"
            names.add(name)
        renamed.append((type_, name))
    return renamed


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
    count_template = await cg.templatable(config[CONF_COUNT], args, cg.uint32)
    shadowed = _shadow_iteration(args)
    then_template = cg.TemplateArguments(cg.uint32, *template_arg.args)
    then_args = [(cg.uint32, "iteration"), *shadowed]
    build_list = _build_action_plan if _is_collecting(action_id) else build_action_list
    actions = await build_list(config[CONF_THEN], then_template, then_args)
    if _is_collecting(action_id) and actions and _plans_inline(actions):
        # The body reads an outer `iteration` under its new name, and the count is read
        # again for every iteration, outside the scope of the body's own `iteration`.
        aliases = [
            f"const auto &{new} = {old};"
            for (_, old), (_, new) in zip(args, shadowed, strict=True)
            if old != new
        ]
        count = immediate_lambda_call(count_template)
        loop_condition = f"esphome__iteration < {count}"
        return _collect_control_flow(
            action_id,
            [
                "{",
                *aliases,
                f"for (uint32_t esphome__iteration = 0; {loop_condition}; esphome__iteration++) {{",
                "const uint32_t iteration = esphome__iteration;",
                *_plan_statements(actions),
                _STOP_CHECK,
                "}",
                "}",
            ],
        )
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_count(count_template))
    for action in _emit_action_plan(actions, then_template, then_args):
        cg.add(var.add_then_action(action))
    cg.add(var.finish_then())
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


async def _build_action_or_inline(
    full_config: ConfigType, template_arg: cg.TemplateArguments, args: TemplateArgsType
) -> MockObj | _InlineAction:
    """Run the action's builder, and return what it collected if it emitted no object.

    An action with a manual ``type_id`` keeps its object: a lambda can reach it by that id.
    """
    action_id = full_config[CONF_TYPE_ID]
    if action_id.is_manual:
        return await build_action(full_config, template_arg, args)
    collector = _InlineCollector(action_id, _builder_name(full_config))
    token = _INLINE_COLLECTOR.set(collector)
    try:
        action = await build_action(full_config, template_arg, args)
    finally:
        _INLINE_COLLECTOR.reset(token)
    _check_collected(action, collector)
    return collector.action if action is _COLLECTED_ACTION else action


def _check_collected(result: MockObj, collector: _InlineCollector) -> None:
    """Returning anything else would silently drop the collected item from the firmware."""
    if (result is _COLLECTED_ACTION) != (collector.action is not None):
        raise EsphomeError(
            f"The builder of {collector.name} must return the result of "
            "its apply builder unchanged"
        )


def _builder_name(full_config: ConfigType) -> str:
    """For an error: the id is usually generated, so the registry key tells the author where to look."""
    name = next(key for key in full_config if key != CONF_TYPE_ID)
    return f"'{name}' ({full_config[CONF_TYPE_ID]})"


async def _build_condition_or_inline(
    full_config: ConfigType, template_arg: cg.TemplateArguments, args: TemplateArgsType
) -> MockObj | _InlineCondition:
    """Condition counterpart of ``_build_action_or_inline``."""
    condition_id = full_config[CONF_TYPE_ID]
    if condition_id.is_manual:
        return await build_condition(full_config, template_arg, args)
    collector = _InlineCollector(condition_id, _builder_name(full_config))
    token = _INLINE_COLLECTOR.set(collector)
    try:
        condition = await build_condition(full_config, template_arg, args)
    finally:
        _INLINE_COLLECTOR.reset(token)
    _check_collected(condition, collector)
    if condition is not _COLLECTED_ACTION:
        return condition
    collected = collector.action
    # ``return <check>;`` from a builder, or a lambda call with its ``;``
    if (
        len(collected.statements) != 1
        or not collected.statements[0].startswith(("return ", "["))
        or not collected.statements[0].endswith(";")
    ):
        raise EsphomeError(
            f"The condition {collector.name} must generate one "
            f"'return <expression>;' statement, not {collected.statements}"
        )
    expression = collected.statements[0].removeprefix("return ").removesuffix(";")
    return _InlineCondition(expression, collected.emit)


def _emit_inline_run(
    run: list[_InlineAction],
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    """Emit one object for adjacent inline actions.

    A single action is emitted as if never collected, unless it is control flow, which has no
    other form once collected.
    """
    if len(run) == 1 and run[0].emit is not None:
        return run[0].emit()
    statements = _run_statements(run)
    id_ = run[0].id_.copy()
    id_.type = ApplyRunAction
    fn = apply_static_function(
        id_,
        cg.void,
        [
            (cg.RawExpression("const int &"), _RUNNING_PARAM),
            *_apply_lambda_args(args),
        ],
        statements,
    )
    return cg.new_Pvariable(id_, cg.TemplateArguments(fn, *template_arg))


async def _build_action_plan(
    config: list[ConfigType], templ: cg.TemplateArguments, arg_type: TemplateArgsType
) -> list[MockObj | list[_InlineAction]]:
    """Build the actions of ``config``; adjacent inline actions stay together, not yet emitted."""
    plan: list[MockObj | list[_InlineAction]] = []
    for conf in config:
        action = await _build_action_or_inline(conf, templ, arg_type)
        if not isinstance(action, _InlineAction):
            plan.append(action)
        elif plan and isinstance(plan[-1], list):
            plan[-1].append(action)
        else:
            plan.append([action])
    return plan


def _emit_action_plan(
    plan: list[MockObj | list[_InlineAction]],
    templ: cg.TemplateArguments,
    arg_type: TemplateArgsType,
) -> list[MockObj]:
    return [
        _emit_inline_run(item, templ, arg_type) if isinstance(item, list) else item
        for item in plan
    ]


async def build_action_list(
    config: list[ConfigType], templ: cg.TemplateArguments, arg_type: TemplateArgsType
) -> list[MockObj]:
    """Build the actions of ``config``, merging adjacent inline actions into one object.

    The result can therefore be shorter than ``config``.
    """
    plan = await _build_action_plan(config, templ, arg_type)
    return _emit_action_plan(plan, templ, arg_type)


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

    Accepts an action list, a single action, or an automation. Only the keys of
    action entries are looked up as action names; the options of an action are
    only searched for nested action lists (``then:``, ``else:``, ...), so an
    option that shares a name with an action, such as the ``delay:`` option of
    ``speaker_source.set_playlist_delay``, is not mistaken for that action.
    """
    if isinstance(actions, list):
        return any(has_non_synchronous_actions(item) for item in actions)
    if isinstance(actions, dict):
        for key, options in actions.items():
            if key in ACTION_REGISTRY and not ACTION_REGISTRY[key].synchronous:
                return True
            if _options_have_non_synchronous_actions(options):
                return True
    return False


def _options_have_non_synchronous_actions(options: Any) -> bool:
    """Search an action's options for nested action lists, without treating option keys as actions."""
    if isinstance(options, list):
        return has_non_synchronous_actions(options)
    if isinstance(options, dict):
        return any(_options_have_non_synchronous_actions(v) for v in options.values())
    return False


def validate_no_deferring_actions(
    context: str, data: str = "the trigger data"
) -> Callable[[ConfigType], ConfigType]:
    """Return a validator that rejects deferring actions in a validated automation.

    Use it for triggers whose arguments only live while the callback runs (views into a reused
    buffer, a JSON document freed on return, ...): deferring actions keep the arguments for later.

    :param context: Where the automation is, for the error message, e.g. ``"on_json and on_data"``.
    :param data: What is only valid while the automation runs, for the error message.
    """

    def validator(value: ConfigType) -> ConfigType:
        if has_non_synchronous_actions(value):
            raise cv.Invalid(
                f"Deferring actions (delay, wait_until, script.wait, ...) are not allowed in "
                f"{context}: {data} is only valid while the automation runs. Copy what you need "
                "into globals first, then defer in a separate script or automation."
            )
        return value

    return validator


def validate_synchronous_automation(
    context: str,
    data: str = "the trigger data",
    extra_schema=None,
    extra_validators=None,
    single: bool = False,
) -> cv.All:
    """``validate_automation()`` that also rejects deferring actions.

    See ``validate_no_deferring_actions()`` for ``context`` and ``data``; the other parameters are
    passed to ``validate_automation()``.
    """
    return cv.All(
        validate_automation(
            extra_schema=extra_schema,
            extra_validators=extra_validators,
            single=single,
        ),
        validate_no_deferring_actions(context, data),
    )


async def _new_automation(
    args: TemplateArgsType, config: ConfigType, *ctor_args: MockObj
) -> MockObj:
    """Create the Automation for ``config`` with its actions."""
    plan = await _build_automation_plan(args, config)
    return _automation_from_plan(plan, args, config, *ctor_args)


async def _build_automation_plan(
    args: TemplateArgsType, config: ConfigType
) -> list[MockObj | list[_InlineAction]]:
    templ = cg.TemplateArguments(*(arg[0] for arg in args))
    return await _build_action_plan(config[CONF_THEN], templ, args)


def _automation_from_plan(
    plan: list[MockObj | list[_InlineAction]],
    args: TemplateArgsType,
    config: ConfigType,
    *ctor_args: MockObj,
) -> MockObj:
    templ = cg.TemplateArguments(*(arg[0] for arg in args))
    obj = cg.new_Pvariable(config[CONF_AUTOMATION_ID], templ, *ctor_args)
    for action in _emit_action_plan(plan, templ, args):
        cg.add(obj.add_action(action))
    return obj


def _is_direct(plan: list[MockObj | list[_InlineAction]], config: ConfigType) -> bool:
    """Whether a callback automation is all inline actions, so it needs no objects.

    Without a Trigger or a manual id nothing can reach such an automation to stop it, so
    no stop checks are needed either. Only for callers that create no Trigger.
    """
    return (
        not config[CONF_AUTOMATION_ID].is_manual
        and len(plan) == 1
        and isinstance(plan[0], list)
    )


def _direct_statements(
    plan: list[MockObj | list[_InlineAction]], config: ConfigType
) -> list[str] | None:
    if not _is_direct(plan, config):
        return None
    return [line for line in _run_statements(plan[0]) if line != _STOP_CHECK]


def _direct_function(
    plan: list[MockObj | list[_InlineAction]],
    args: TemplateArgsType,
    config: ConfigType,
) -> MockObj | None:
    if (statements := _direct_statements(plan, config)) is None:
        return None
    return apply_static_function(
        plan[0][0].id_, cg.void, _apply_lambda_args(args), statements
    )


def _when_call(
    when: str | ApplyCall | None, parent: MockObj | None
) -> ApplyCall | None:
    if when is None:
        return None
    call = when if isinstance(when, ApplyCall) else ApplyCall(when)
    if parent is None and call.names_parent:
        raise ValueError(f"trigger filter {call.target!r} names {{parent}}")
    return call


async def build_automation(
    trigger: MockObj, args: TemplateArgsType, config: ConfigType
) -> MockObj:
    return await _new_automation(args, config, trigger)


async def build_component_automation(
    config: ConfigType, component_class: MockObjClass, priority: float | None = None
) -> MockObj:
    """Create and register the Component that runs a no-argument automation.

    That is ``component_class<function[, priority bits]>``, whose function runs the actions
    directly when they are all inline, else triggers their Automation. A ``trigger_id`` set
    by hand keeps its Trigger, built with ``priority`` if given.
    """
    trigger_id: ID = config[CONF_TRIGGER_ID]
    if trigger_id.is_manual:
        var = cg.new_Pvariable(trigger_id, *([] if priority is None else [priority]))
        await cg.register_component(var, config)
        await build_automation(var, [], config)
        return var
    # Declared and registered before the actions are built, which can wait for other ids
    name = f"esphome__{trigger_id.id}__fn"
    define = cg.declare_static_function(name, cg.void, [])
    id_ = trigger_id.copy()
    id_.type = component_class
    bits = [] if priority is None else [_float_bits(priority)]
    var = cg.new_Pvariable(id_, cg.TemplateArguments(cg.RawExpression(name), *bits))
    await cg.register_component(var, config)
    plan = await _build_automation_plan([], config)
    if (body := _direct_statements(plan, config)) is None:
        body = [f"{parent_ref(_automation_from_plan(plan, [], config))}->trigger();"]
    define(body)
    return var


def _float_bits(value: float) -> cg.RawExpression:
    """``value`` as a C++ float's bit pattern, for a template argument."""
    (bits,) = struct.unpack("<I", struct.pack("<f", value))
    return cg.RawExpression(f"0x{bits:08X}u")


async def build_trigger_callback(
    args: TemplateArgsType,
    config: ConfigType,
    params: TemplateArgsType,
    forward: Sequence[str | Expression] | None = None,
    when: str | ApplyCall | None = None,
    parent: MockObj | None = None,
) -> LambdaExpression:
    """Build ``config`` and return a stateless callback that runs its actions.

    An automation that is all inline actions becomes a function the callback calls, with no
    Automation or action objects; otherwise the callback triggers an Automation.

    ``params`` are the parent callback's parameters, ``forward`` the expressions passed to
    the actions (default: the parameter names; write the parent as ``parent_ref(var)``),
    ``when`` a filter the callback returns early on, skipped like any ``ApplyCall`` when none
    of its keys is set. ``when`` may name ``parent`` as ``{parent}``, e.g.
    ``"{parent}->is_fully_open()"``.
    """
    call = _when_call(when, parent)
    plan = await _build_automation_plan(args, config)
    return await _trigger_callback(plan, args, config, params, forward, call, parent)


async def _trigger_callback(
    plan: list[MockObj | list[_InlineAction]],
    args: TemplateArgsType,
    config: ConfigType,
    params: TemplateArgsType,
    forward: Sequence[str | Expression] | None,
    call: ApplyCall | None,
    parent: MockObj | None,
) -> LambdaExpression:
    if (target := _direct_function(plan, args, config)) is None:
        target = f"{parent_ref(_automation_from_plan(plan, args, config))}->trigger"
    lambda_args = _apply_lambda_args(params)
    statements: list[str] = []
    if call is not None:
        members = call.members
        values = _apply_values(config, members)
        if _apply_call_active(members, values):
            check = await _render_check(
                "trigger filter",
                call.target,
                members,
                values,
                config,
                None if parent is None else str(parent_ref(parent)),
                lambda_args,
            )
            statements.append(f"if (!({check}))\n  return;")
    if forward is None:
        forward = [name for _, name in params]
    statements.append(f"{target}({', '.join(map(str, forward))});")
    return LambdaExpression(
        ["\n".join(statements)], lambda_args, capture="", return_type=cg.void
    )


async def build_callback_automation(
    parent: MockObj,
    callback_method: str,
    args: TemplateArgsType,
    config: ConfigType,
    forwarder: MockObj | MockObjClass | None = None,
    params: TemplateArgsType | None = None,
    forward: Sequence[str | Expression] | None = None,
    when: str | ApplyCall | None = None,
) -> None:
    """Build an Automation and register it as a callback on the parent.

    Eliminates the need for a Trigger wrapper object by registering the
    automation's trigger() directly as a callback on the parent component.

    Uses template forwarder structs so the compiler deduplicates the operator()
    body across all call sites with the same signature. The forwarder must be
    pointer-sized (single Automation* field) to fit inline in Callback::ctx_
    and avoid heap allocation.

    With ``params``, ``forward`` or ``when`` the callback is instead the stateless
    lambda of ``build_trigger_callback``. An automation that is all inline actions needs
    no objects and is called directly, so it also takes that lambda when no filter is
    given. A ``forwarder`` given with them is used when the automation does need objects;
    they must then describe the same filter. A ``forwarder`` alone keeps its objects.

    :param parent: The component object (e.g., button, sensor).
    :param callback_method: Name of the callback method (e.g., "add_on_press_callback").
    :param args: Automation template args as list of (type, name) tuples.
    :param config: The automation config dict.
    :param forwarder: Optional forwarder type to use instead of the default
        TriggerForwarder<Ts...>. Pass any struct type whose aggregate init takes
        a single Automation pointer (e.g., TriggerOnTrueForwarder).
    """
    custom = params is not None or forward is not None or when is not None
    if not custom and (
        forwarder is TriggerOnTrueForwarder or forwarder is TriggerOnFalseForwarder
    ):
        if args:
            raise ValueError(f"{forwarder} triggers an automation with no arguments")
        params, forward, custom = [(cg.bool_, "state")], [], True
        when = "state" if forwarder is TriggerOnTrueForwarder else "!state"
    call = _when_call(when, parent)
    plan = await _build_automation_plan(args, config)
    # A forwarder alone says nothing about what it filters, so it cannot be replaced
    if (custom and forwarder is None) or (
        (custom or forwarder is None) and _is_direct(plan, config)
    ):
        callback = await _trigger_callback(
            plan,
            args,
            config,
            args if params is None else params,
            forward,
            call,
            parent,
        )
        cg.add(getattr(parent, callback_method)(callback))
        return
    obj = _automation_from_plan(plan, args, config)
    # Use template forwarder structs for deduplication. The compiler generates
    # one operator() per forwarder type; different automation pointers are just
    # data in the struct.
    if forwarder is None:
        forwarder = TriggerForwarder.template(*(arg[0] for arg in args))
    # RawExpression for aggregate init — both forwarder and obj are codegen
    # MockObjs (not user input), and there's no Expression type for positional
    # aggregate initialization (StructInitializer uses named fields).
    cg.add(getattr(parent, callback_method)(cg.RawExpression(f"{forwarder}{{{obj}}}")))


async def build_parent_callback_automation(
    parent: MockObj, callback_method: str, arg: tuple[Any, str], config: ConfigType
) -> None:
    """Register an Automation that receives ``parent`` from a callback that carries nothing.

    ``arg`` is the automation's ``(type, name)``, e.g. ``(Fan.operator("ptr"), "x")``.
    """
    await build_callback_automation(
        parent, callback_method, [arg], config, params=[], forward=[parent_ref(parent)]
    )


async def build_trigger_automations(
    parent: MockObj | None,
    config: ConfigType,
    entries: tuple[tuple[str, TemplateArgsType], ...],
) -> None:
    """Instantiate each entry's Trigger class, with ``parent`` when given, and build its automations.

    ``entries`` are ``(conf_key, args)`` pairs; the class comes from the entry's ``CONF_TRIGGER_ID``.
    """
    ctor_args = () if parent is None else (parent,)
    for conf_key, args in entries:
        for conf in config.get(conf_key, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], *ctor_args)
            await build_automation(trigger, args, conf)


@dataclass(frozen=True, slots=True)
class CallbackAutomation:
    """A single callback automation entry for build_callback_automations."""

    conf_key: str
    callback_method: str
    args: TemplateArgsType = field(default_factory=list)
    forwarder: MockObj | MockObjClass | None = None
    params: TemplateArgsType | None = None
    forward: Sequence[str | Expression] | None = None
    when: str | ApplyCall | None = None


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
                params=entry.params,
                forward=entry.forward,
                when=entry.when,
            )

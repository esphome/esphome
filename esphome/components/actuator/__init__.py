"""Actuator base component.

This module defines the shared base classes for actuator-type devices
(Cover, Valve). It provides no YAML configuration on its own.
"""

from collections.abc import Callable
from dataclasses import dataclass

import esphome.codegen as cg
from esphome.const import CONF_ID
from esphome.core import ID, Lambda
from esphome.cpp_generator import LambdaExpression, MockObj, RawExpression
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@esphome/core", "@exciton"]

actuator_ns = cg.esphome_ns.namespace("actuator")

# C++ classes
ActuatorBase = actuator_ns.class_("ActuatorBase", cg.EntityBase)
ActuatorCallBase = actuator_ns.class_("ActuatorCallBase")
IActuator = actuator_ns.class_("IActuator")

# Constants exposed as raw C++ float literals so that str() == "1.0f" / "0.0f"
ACTUATOR_OPEN = RawExpression("1.0f")
ACTUATOR_CLOSED = RawExpression("0.0f")

# ActuatorOperation enum — expose as a namespace object so callers can do
# ActuatorOperation.ACTUATOR_OPERATION_IDLE etc.
# Members render as qualified C++ names so generated code can assign them to
# ActuatorOperation fields without implicit int-to-enum conversion errors.


class _ActuatorOperationEnum:
    """Lightweight stand-in for the C++ ActuatorOperation enum.

    Attributes render as qualified C++ names (e.g. "esphome::actuator::ACTUATOR_OPERATION_IDLE")
    so they can be emitted directly into generated code without implicit int-to-enum conversions.
    """

    ACTUATOR_OPERATION_IDLE = actuator_ns.ACTUATOR_OPERATION_IDLE
    ACTUATOR_OPERATION_OPENING = actuator_ns.ACTUATOR_OPERATION_OPENING
    ACTUATOR_OPERATION_CLOSING = actuator_ns.ACTUATOR_OPERATION_CLOSING

    def __str__(self):
        return "actuator::ActuatorOperation"


ActuatorOperation = _ActuatorOperationEnum()


@dataclass(frozen=True)
class ApplyField:
    """One field in a folded-lambda action.

    `conf_key` is the YAML key looked up in `config`. When present, the
    helper emits `statement_fn(target, value_expr)` into the lambda body.
    `target` is whatever the statement function needs to identify the
    field (typically a setter name like `"set_position"` or a struct
    member like `"position"`). `type_` is the C++ return type for
    `cg.process_lambda` when the value is a user lambda.
    """

    conf_key: str
    target: str
    type_: object


async def build_apply_lambda_action(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
    fields: tuple["ApplyField", ...],
    prefix_args: list[tuple[object, str]],
    statement_fn: Callable[[str, str], str],
) -> MockObj:
    """Fold configured fields into a single stateless apply lambda action.

    Constants are emitted as flash immediates; user lambdas are invoked
    inline so trigger args still flow. Trigger arg types are normalized to
    `const std::remove_cvref_t<T> &` to match the ApplyFn signature for
    any T (value, ref, or const-ref).
    """
    paren = await cg.get_variable(config[CONF_ID])
    normalized_args = [
        (cg.RawExpression(f"const std::remove_cvref_t<{cg.safe_exp(t)}> &"), n)
        for t, n in args
    ]
    fwd_args = ", ".join(name for _, name in args)
    body_lines: list[str] = []
    for field in fields:
        if (value := config.get(field.conf_key)) is None:
            continue
        if isinstance(value, Lambda):
            inner = await cg.process_lambda(
                value, normalized_args, return_type=field.type_
            )
            value_expr = f"({inner})({fwd_args})"
        else:
            value_expr = str(cg.safe_exp(value))
        body_lines.append(statement_fn(field.target, value_expr))
    apply_lambda = LambdaExpression(
        ["\n".join(body_lines)],
        [*prefix_args, *normalized_args],
        capture="",
        return_type=cg.void,
    )
    return cg.new_Pvariable(action_id, template_arg, paren, apply_lambda)

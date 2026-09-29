from collections.abc import Callable
from typing import Any

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCELERATION,
    CONF_DECELERATION,
    CONF_ID,
    CONF_MAX_SPEED,
    CONF_POSITION,
    CONF_SPEED,
    CONF_TARGET,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import SafeExpType

IS_PLATFORM_COMPONENT = True

stepper_ns = cg.esphome_ns.namespace("stepper")
Stepper = stepper_ns.class_("Stepper")


def validate_acceleration(value):
    value = cv.string(value)
    for suffix in ("steps/s^2", "steps/s*s", "steps/s/s", "steps/ss", "steps/(s*s)"):
        value = value.removesuffix(suffix)

    if value == "inf":
        return 1e6

    try:
        value = float(value)
    except ValueError:
        raise cv.Invalid(
            f"Expected acceleration as floating point number, got {value}"
        ) from None

    if value <= 0:
        raise cv.Invalid("Acceleration must be larger than 0 steps/s^2!")

    return value


def validate_speed(value):
    value = cv.string(value)
    for suffix in ("steps/s",):
        value = value.removesuffix(suffix)

    if value == "inf":
        return 1e6

    try:
        value = float(value)
    except ValueError:
        raise cv.Invalid(
            f"Expected speed as floating point number, got {value}"
        ) from None

    if value <= 0:
        raise cv.Invalid("Speed must be larger than 0 steps/s!")

    return value


STEPPER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_MAX_SPEED): validate_speed,
        cv.Optional(CONF_ACCELERATION, default="inf"): validate_acceleration,
        cv.Optional(CONF_DECELERATION, default="inf"): validate_acceleration,
    }
)


async def setup_stepper_core_(stepper_var, config):
    if CONF_ACCELERATION in config:
        cg.add(stepper_var.set_acceleration(config[CONF_ACCELERATION]))
    if CONF_DECELERATION in config:
        cg.add(stepper_var.set_deceleration(config[CONF_DECELERATION]))
    if CONF_MAX_SPEED in config:
        cg.add(stepper_var.set_max_speed(config[CONF_MAX_SPEED]))


async def register_stepper(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    await setup_stepper_core_(var, config)


def _register_stepper_action(
    name: str,
    key: str,
    validator: Callable[[Any], Any],
    target: str,
    type_: SafeExpType,
    *extra: automation.ApplyCall,
) -> None:
    automation.register_apply_action(
        f"stepper.{name}",
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(Stepper),
                cv.Required(key): cv.templatable(validator),
            }
        ),
        automation.ApplyField(key, target, type_),
        *extra,
    )


_register_stepper_action("set_target", CONF_TARGET, cv.int_, "set_target", cg.int32)
_register_stepper_action(
    "report_position", CONF_POSITION, cv.int_, "report_position", cg.int32
)
_register_stepper_action(
    "set_speed",
    CONF_SPEED,
    validate_speed,
    "set_max_speed",
    cg.float_,
    automation.ApplyCall("on_update_speed()"),
)
_register_stepper_action(
    "set_acceleration",
    CONF_ACCELERATION,
    validate_acceleration,
    "set_acceleration",
    cg.float_,
)
_register_stepper_action(
    "set_deceleration",
    CONF_DECELERATION,
    validate_acceleration,
    "set_deceleration",
    cg.float_,
)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(stepper_ns.using)

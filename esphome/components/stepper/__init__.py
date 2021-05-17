import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ACCELERATION,
    CONF_DECELERATION,
    CONF_ID,
    CONF_MAX_SPEED,
    CONF_POSITION,
    CONF_TARGET,
    CONF_SPEED,
)
from esphome.core import CORE, coroutine, coroutine_with_priority

IS_PLATFORM_COMPONENT = True

# pylint: disable=invalid-name
stepper_ns = cg.esphome_ns.namespace("stepper")
Stepper = stepper_ns.class_("Stepper")

SetTargetAction = stepper_ns.class_("SetTargetAction", automation.Action)
ReportPositionAction = stepper_ns.class_("ReportPositionAction", automation.Action)
SetSpeedAction = stepper_ns.class_("SetSpeedAction", automation.Action)


def validate_acceleration(value):
    value = cv.string(value)
    for suffix in ("steps/s^2", "steps/s*s", "steps/s/s", "steps/ss", "steps/(s*s)"):
        if value.endswith(suffix):
            value = value[: -len(suffix)]

    if value == "inf":
        return 1e6

    try:
        value = float(value)
    except ValueError:
        # pylint: disable=raise-missing-from
        raise cv.Invalid(f"Expected acceleration as floating point number, got {value}")

    if value <= 0:
        raise cv.Invalid("Acceleration must be larger than 0 steps/s^2!")

    return value


def validate_speed(value):
    value = cv.string(value)
    for suffix in ("steps/s", "steps/s"):
        if value.endswith(suffix):
            value = value[: -len(suffix)]

    if value == "inf":
        return 1e6

    try:
        value = float(value)
    except ValueError:
        # pylint: disable=raise-missing-from
        raise cv.Invalid(f"Expected speed as floating point number, got {value}")

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


@coroutine
def setup_stepper_core_(stepper_var, config):
    if CONF_ACCELERATION in config:
        cg.add(stepper_var.set_acceleration(config[CONF_ACCELERATION]))
    if CONF_DECELERATION in config:
        cg.add(stepper_var.set_deceleration(config[CONF_DECELERATION]))
    if CONF_MAX_SPEED in config:
        cg.add(stepper_var.set_max_speed(config[CONF_MAX_SPEED]))


@coroutine
def register_stepper(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    yield setup_stepper_core_(var, config)


@automation.register_action(
    "stepper.set_target",
    SetTargetAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(Stepper),
            cv.Required(CONF_TARGET): cv.templatable(cv.int_),
        }
    ),
)
def stepper_set_target_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_TARGET], args, cg.int32)
    cg.add(var.set_target(template_))
    yield var


@automation.register_action(
    "stepper.report_position",
    ReportPositionAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(Stepper),
            cv.Required(CONF_POSITION): cv.templatable(cv.int_),
        }
    ),
)
def stepper_report_position_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_POSITION], args, cg.int32)
    cg.add(var.set_position(template_))
    yield var


@automation.register_action(
    "stepper.set_speed",
    SetSpeedAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(Stepper),
            cv.Required(CONF_SPEED): cv.templatable(validate_speed),
        }
    ),
)
def stepper_set_speed_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_SPEED], args, cg.int32)
    cg.add(var.set_speed(template_))
    yield var


@coroutine_with_priority(100.0)
def to_code(config):
    cg.add_global(stepper_ns.using)

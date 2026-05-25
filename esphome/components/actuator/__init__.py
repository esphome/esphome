"""Actuator base component.

This module defines the shared base classes for actuator-type devices
(Cover, Valve). It provides no YAML configuration on its own.
"""

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ASSUMED_STATE,
    CONF_CLOSE_ACTION,
    CONF_CLOSE_DURATION,
    CONF_OPEN_ACTION,
    CONF_OPEN_DURATION,
    CONF_STOP_ACTION,
)
from esphome.cpp_generator import RawExpression

CONF_HAS_BUILT_IN_ENDSTOP = "has_built_in_endstop"
CONF_MANUAL_CONTROL = "manual_control"

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
# Use RawExpression for each member so that str() yields the integer string
# ("0", "1", "2") required by the unit tests and matching what C++ enums resolve to.


class _ActuatorOperationEnum:
    """Lightweight stand-in for the C++ ActuatorOperation enum.

    Attributes are exposed as RawExpression so their str() equals the
    underlying integer value ("0", "1", "2"), matching C++ enum promotion.
    """

    ACTUATOR_OPERATION_IDLE = RawExpression("0")
    ACTUATOR_OPERATION_OPENING = RawExpression("1")
    ACTUATOR_OPERATION_CLOSING = RawExpression("2")

    def __str__(self):
        return "actuator::ActuatorOperation"


ActuatorOperation = _ActuatorOperationEnum()

TimeBasedActuatorBase = actuator_ns.class_("TimeBasedActuatorBase", cg.Component)

TIME_BASED_ACTUATOR_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),
        cv.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
        cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,
        cv.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
        cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_HAS_BUILT_IN_ENDSTOP, default=False): cv.boolean,
        cv.Optional(CONF_MANUAL_CONTROL, default=False): cv.boolean,
        cv.Optional(CONF_ASSUMED_STATE, default=True): cv.boolean,
    }
)


async def apply_time_based_actuator_config(var, config):
    await automation.build_automation(
        var.get_stop_trigger(), [], config[CONF_STOP_ACTION]
    )
    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    await automation.build_automation(
        var.get_open_trigger(), [], config[CONF_OPEN_ACTION]
    )
    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    await automation.build_automation(
        var.get_close_trigger(), [], config[CONF_CLOSE_ACTION]
    )
    cg.add(var.set_has_built_in_endstop(config[CONF_HAS_BUILT_IN_ENDSTOP]))
    cg.add(var.set_manual_control(config[CONF_MANUAL_CONTROL]))
    cg.add(var.set_assumed_state(config[CONF_ASSUMED_STATE]))

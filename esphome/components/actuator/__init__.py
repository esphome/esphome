"""Actuator base component.

This module defines the shared base classes for actuator-type devices
(Cover, Valve). It provides no YAML configuration on its own.
"""

import esphome.codegen as cg
from esphome.cpp_generator import RawExpression

CODEOWNERS = ["@esphome/core"]

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

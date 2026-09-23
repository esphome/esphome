"""Actuator base component.

This module defines the shared base classes for actuator-type devices
(Cover, Valve). It provides no YAML configuration on its own.
"""

import esphome.codegen as cg
from esphome.cpp_generator import RawExpression

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

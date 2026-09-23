"""Actuator base component.

This module defines the shared base classes for actuator-type devices
(Cover, Valve). It provides no YAML configuration on its own.
"""

import esphome.codegen as cg

CODEOWNERS = ["@esphome/core", "@exciton"]

actuator_ns = cg.esphome_ns.namespace("actuator")

# C++ classes
ActuatorBase = actuator_ns.class_("ActuatorBase", cg.EntityBase)
ActuatorCallBase = actuator_ns.class_("ActuatorCallBase")
IActuator = actuator_ns.class_("IActuator")

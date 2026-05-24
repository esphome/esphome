"""Actuator base component.

This module defines the shared base classes for actuator-type devices
(Cover, Valve). It provides no YAML configuration on its own.
"""

from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLOSE_ACTION,
    CONF_CLOSE_DURATION,
    CONF_CLOSE_ENDSTOP,
    CONF_MAX_DURATION,
    CONF_OPEN_ACTION,
    CONF_OPEN_DURATION,
    CONF_OPEN_ENDSTOP,
    CONF_STOP_ACTION,
)
from esphome.cpp_generator import RawExpression

CODEOWNERS = ["@esphome/core"]

actuator_ns = cg.esphome_ns.namespace("actuator")

# C++ classes
ActuatorBase = actuator_ns.class_("ActuatorBase", cg.EntityBase)
ActuatorCallBase = actuator_ns.class_("ActuatorCallBase")
IActuator = actuator_ns.class_("IActuator")
EndstopActuatorBase = actuator_ns.class_("EndstopActuatorBase", cg.Component)

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

ENDSTOP_ACTUATOR_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),
        cv.Required(CONF_OPEN_ENDSTOP): cv.use_id(binary_sensor.BinarySensor),
        cv.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
        cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,
        cv.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
        cv.Required(CONF_CLOSE_ENDSTOP): cv.use_id(binary_sensor.BinarySensor),
        cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_DURATION): cv.positive_time_period_milliseconds,
    }
)


async def apply_endstop_actuator_config(var, config):
    await automation.build_automation(
        var.get_stop_trigger(), [], config[CONF_STOP_ACTION]
    )
    bin = await cg.get_variable(config[CONF_OPEN_ENDSTOP])
    cg.add(var.set_open_endstop(bin))
    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    await automation.build_automation(
        var.get_open_trigger(), [], config[CONF_OPEN_ACTION]
    )
    bin = await cg.get_variable(config[CONF_CLOSE_ENDSTOP])
    cg.add(var.set_close_endstop(bin))
    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    await automation.build_automation(
        var.get_close_trigger(), [], config[CONF_CLOSE_ACTION]
    )
    if CONF_MAX_DURATION in config:
        cg.add(var.set_max_duration(config[CONF_MAX_DURATION]))

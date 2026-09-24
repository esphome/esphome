import esphome.codegen as cg
from esphome.components import valve
import esphome.config_validation as cv

from .. import (
    ENDSTOP_ACTUATOR_SCHEMA,
    EndstopActuatorBase,
    apply_endstop_actuator_config,
    endstop_ns,
)

# Auto-load the parent endstop component so its source files
# (endstop_actuator.cpp/.h) are picked up by the build.
AUTO_LOAD = ["endstop"]

EndstopValve = endstop_ns.class_("EndstopValve", EndstopActuatorBase, valve.Valve)

CONFIG_SCHEMA = (
    valve.valve_schema(EndstopValve)
    .extend(ENDSTOP_ACTUATOR_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await valve.new_valve(config)
    await cg.register_component(var, config)
    await apply_endstop_actuator_config(var, config)

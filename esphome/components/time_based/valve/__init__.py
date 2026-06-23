import esphome.codegen as cg
from esphome.components import valve
import esphome.config_validation as cv

from .. import (
    TIME_BASED_ACTUATOR_SCHEMA,
    TimeBasedActuatorBase,
    apply_time_based_actuator_config,
    time_based_ns,
)

AUTO_LOAD = ["time_based"]

TimeBasedValve = time_based_ns.class_(
    "TimeBasedValve", TimeBasedActuatorBase, valve.Valve
)

CONFIG_SCHEMA = (
    valve.valve_schema(TimeBasedValve)
    .extend(TIME_BASED_ACTUATOR_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await valve.new_valve(config)
    await cg.register_component(var, config)
    await apply_time_based_actuator_config(var, config)

import esphome.codegen as cg
from esphome.components import actuator as actuator_component, valve
import esphome.config_validation as cv

from .. import time_based_valve_ns

DEPENDENCIES = ["valve", "actuator"]

TimeBasedValve = time_based_valve_ns.class_(
    "TimeBasedValve", actuator_component.TimeBasedActuatorBase, valve.Valve
)

CONFIG_SCHEMA = (
    valve.valve_schema(TimeBasedValve)
    .extend(actuator_component.TIME_BASED_ACTUATOR_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await valve.new_valve(config)
    await cg.register_component(var, config)
    await actuator_component.apply_time_based_actuator_config(var, config)

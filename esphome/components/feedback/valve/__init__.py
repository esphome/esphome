import esphome.codegen as cg
from esphome.components import valve
import esphome.config_validation as cv

from .. import (
    FEEDBACK_ACTUATOR_SCHEMA,
    FEEDBACK_ACTUATOR_VALIDATORS,
    FeedbackActuatorBase,
    apply_feedback_actuator_config,
    feedback_ns,
)

# Auto-load the parent feedback component so its source files
# (feedback_actuator.cpp/.h) are picked up by the build.
AUTO_LOAD = ["feedback"]

FeedbackValve = feedback_ns.class_("FeedbackValve", FeedbackActuatorBase, valve.Valve)

CONFIG_SCHEMA = cv.All(
    valve.valve_schema(FeedbackValve)
    .extend(FEEDBACK_ACTUATOR_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    *FEEDBACK_ACTUATOR_VALIDATORS,
)


async def to_code(config):
    var = await valve.new_valve(config)
    await cg.register_component(var, config)
    await apply_feedback_actuator_config(var, config)

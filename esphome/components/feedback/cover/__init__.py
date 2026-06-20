import esphome.codegen as cg
from esphome.components import cover
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

FeedbackCover = feedback_ns.class_("FeedbackCover", FeedbackActuatorBase, cover.Cover)

CONFIG_SCHEMA = cv.All(
    cover.cover_schema(FeedbackCover)
    .extend(FEEDBACK_ACTUATOR_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    *FEEDBACK_ACTUATOR_VALIDATORS,
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    await apply_feedback_actuator_config(var, config)

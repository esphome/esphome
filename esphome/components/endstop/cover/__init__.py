import esphome.codegen as cg
from esphome.components import cover
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

EndstopCover = endstop_ns.class_("EndstopCover", EndstopActuatorBase, cover.Cover)

CONFIG_SCHEMA = (
    cover.cover_schema(EndstopCover)
    .extend(ENDSTOP_ACTUATOR_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    await apply_endstop_actuator_config(var, config)

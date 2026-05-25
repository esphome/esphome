import esphome.codegen as cg
from esphome.components import cover
import esphome.config_validation as cv

from . import (
    TIME_BASED_ACTUATOR_SCHEMA,
    TimeBasedActuatorBase,
    apply_time_based_actuator_config,
    time_based_ns,
)

TimeBasedCover = time_based_ns.class_(
    "TimeBasedCover", TimeBasedActuatorBase, cover.Cover
)

CONFIG_SCHEMA = (
    cover.cover_schema(TimeBasedCover)
    .extend(TIME_BASED_ACTUATOR_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    await apply_time_based_actuator_config(var, config)

import esphome.codegen as cg
from esphome.components import actuator as actuator_component, cover
import esphome.config_validation as cv

from .. import time_based_ns

TimeBasedCover = time_based_ns.class_(
    "TimeBasedCover", actuator_component.TimeBasedActuatorBase, cover.Cover
)

CONFIG_SCHEMA = (
    cover.cover_schema(TimeBasedCover)
    .extend(actuator_component.TIME_BASED_ACTUATOR_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    await actuator_component.apply_time_based_actuator_config(var, config)

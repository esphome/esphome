import esphome.codegen as cg
from esphome.components import cover
from esphome.types import ConfigType

from .. import ENDSTOP_SCHEMA, endstop_ns, setup_endstop

# Load the parent endstop component so the shared endstop_actuator.h is part of the build.
AUTO_LOAD = ["endstop"]

EndstopCover = endstop_ns.class_("EndstopCover", cover.Cover, cg.Component)

CONFIG_SCHEMA = cover.cover_schema(EndstopCover).extend(ENDSTOP_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = await cover.new_cover(config)
    await setup_endstop(var, config)

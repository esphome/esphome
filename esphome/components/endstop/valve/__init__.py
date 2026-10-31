import esphome.codegen as cg
from esphome.components import valve
from esphome.types import ConfigType

from .. import ENDSTOP_SCHEMA, endstop_ns, setup_endstop

# Load the parent endstop component so the shared endstop_actuator.h is part of the build.
AUTO_LOAD = ["endstop"]

EndstopValve = endstop_ns.class_("EndstopValve", valve.Valve, cg.Component)

CONFIG_SCHEMA = valve.valve_schema(EndstopValve).extend(ENDSTOP_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = await valve.new_valve(config)
    await setup_endstop(var, config)

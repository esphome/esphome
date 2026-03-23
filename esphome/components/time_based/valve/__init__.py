import esphome.codegen as cg
from esphome.components import valve
import esphome.config_validation as cv

time_based_ns = cg.esphome_ns.namespace("time_based")
TimeBasedValve = time_based_ns.class_("TimeBasedValve", valve.Valve, cg.Component)

CONFIG_SCHEMA = valve.valve_schema(TimeBasedValve).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await valve.new_valve(config)
    await cg.register_component(var, config)

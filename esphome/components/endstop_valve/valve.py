import esphome.codegen as cg
from esphome.components import valve
from esphome.components.endstop import (
    ENDSTOP_ACTUATOR_SCHEMA,
    EndstopActuatorBase,
    apply_endstop_actuator_config,
)
import esphome.config_validation as cv

DEPENDENCIES = ["valve", "endstop"]

endstop_valve_ns = cg.esphome_ns.namespace("endstop_valve")
EndstopValve = endstop_valve_ns.class_("EndstopValve", EndstopActuatorBase, valve.Valve)

CONFIG_SCHEMA = (
    valve.valve_schema(EndstopValve)
    .extend(ENDSTOP_ACTUATOR_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await valve.new_valve(config)
    await cg.register_component(var, config)
    await apply_endstop_actuator_config(var, config)

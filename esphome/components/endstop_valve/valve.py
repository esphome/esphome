import esphome.codegen as cg
from esphome.components import actuator as actuator_component, valve
import esphome.config_validation as cv

DEPENDENCIES = ["valve", "actuator"]

endstop_valve_ns = cg.esphome_ns.namespace("endstop_valve")
EndstopValve = endstop_valve_ns.class_(
    "EndstopValve", actuator_component.EndstopActuatorBase, valve.Valve
)

CONFIG_SCHEMA = (
    valve.valve_schema(EndstopValve)
    .extend(actuator_component.ENDSTOP_ACTUATOR_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await valve.new_valve(config)
    await cg.register_component(var, config)
    await actuator_component.apply_endstop_actuator_config(var, config)

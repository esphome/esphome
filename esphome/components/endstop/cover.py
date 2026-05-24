import esphome.codegen as cg
from esphome.components import actuator as actuator_component, cover
import esphome.config_validation as cv

endstop_ns = cg.esphome_ns.namespace("endstop")
EndstopCover = endstop_ns.class_(
    "EndstopCover", actuator_component.EndstopActuatorBase, cover.Cover
)

CONFIG_SCHEMA = (
    cover.cover_schema(EndstopCover)
    .extend(actuator_component.ENDSTOP_ACTUATOR_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    await actuator_component.apply_endstop_actuator_config(var, config)

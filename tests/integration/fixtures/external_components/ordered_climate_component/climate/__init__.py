"""Climate platform that exposes fan modes in a custom order."""

import esphome.codegen as cg
from esphome.components import climate
import esphome.config_validation as cv
from esphome.types import ConfigType

ordered_climate_ns = cg.esphome_ns.namespace("ordered_climate_test")
OrderedClimate = ordered_climate_ns.class_(
    "OrderedClimate", climate.Climate, cg.Component
)

CONFIG_SCHEMA = climate.climate_schema(OrderedClimate).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = await climate.new_climate(config)
    await cg.register_component(var, config)

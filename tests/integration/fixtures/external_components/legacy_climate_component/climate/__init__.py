"""Legacy climate platform that uses deprecated ClimateTraits setters."""

import esphome.codegen as cg
from esphome.components import climate
import esphome.config_validation as cv
from esphome.types import ConfigType

legacy_climate_ns = cg.esphome_ns.namespace("legacy_climate_test")
LegacyClimate = legacy_climate_ns.class_("LegacyClimate", climate.Climate, cg.Component)

CONFIG_SCHEMA = climate.climate_schema(LegacyClimate).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = await climate.new_climate(config)
    await cg.register_component(var, config)

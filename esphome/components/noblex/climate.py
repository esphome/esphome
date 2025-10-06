import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]

noblex_ns = cg.esphome_ns.namespace("noblex")
NoblexClimate = noblex_ns.class_("NoblexClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(NoblexClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

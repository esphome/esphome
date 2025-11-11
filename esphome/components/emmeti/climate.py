import esphome.codegen as cg
from esphome.components import climate_ir

CODEOWNERS = ["@E440QF"]
AUTO_LOAD = ["climate_ir"]

emmeti_ns = cg.esphome_ns.namespace("emmeti")
EmmetiClimate = emmeti_ns.class_("EmmetiClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(EmmetiClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

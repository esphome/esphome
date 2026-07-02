import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@arpena"]

hisense_arg_ns = cg.esphome_ns.namespace("hisense_arg")
HisenseArgClimate = hisense_arg_ns.class_("HisenseArgClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(HisenseArgClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

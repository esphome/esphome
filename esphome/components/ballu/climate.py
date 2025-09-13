import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@bazuchan"]

ballu_ns = cg.esphome_ns.namespace("ballu")
BalluClimate = ballu_ns.class_("BalluClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(BalluClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

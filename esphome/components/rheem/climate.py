import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@amet"]

rheem_ns = cg.esphome_ns.namespace("rheem")
RheemClimate = rheem_ns.class_("RheemClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(RheemClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]

fujitsu_general_ns = cg.esphome_ns.namespace("fujitsu_general")
FujitsuGeneralClimate = fujitsu_general_ns.class_(
    "FujitsuGeneralClimate", climate_ir.ClimateIR
)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(FujitsuGeneralClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

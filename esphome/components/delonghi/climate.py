import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]

delonghi_ns = cg.esphome_ns.namespace("delonghi")
DelonghiClimate = delonghi_ns.class_("DelonghiClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(DelonghiClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

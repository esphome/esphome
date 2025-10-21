import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]

daikin_ns = cg.esphome_ns.namespace("daikin")
DaikinClimate = daikin_ns.class_("DaikinClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(DaikinClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

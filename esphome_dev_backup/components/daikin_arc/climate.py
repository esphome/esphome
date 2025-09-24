import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]

daikin_arc_ns = cg.esphome_ns.namespace("daikin_arc")
DaikinArcClimate = daikin_arc_ns.class_("DaikinArcClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(DaikinArcClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

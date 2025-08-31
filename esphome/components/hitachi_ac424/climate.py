import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]

hitachi_ac424_ns = cg.esphome_ns.namespace("hitachi_ac424")
HitachiClimate = hitachi_ac424_ns.class_("HitachiClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(HitachiClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

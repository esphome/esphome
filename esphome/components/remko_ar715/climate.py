import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]

remko_ar715_ns = cg.esphome_ns.namespace("remko_ar715")
RemkoAr715Climate = remko_ar715_ns.class_("RemkoAr715Climate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_schema(RemkoAr715Climate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

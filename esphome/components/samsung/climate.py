import esphome.codegen as cg
from esphome.components import climate_ir

CODEOWNERS = ["@jorofi"]

AUTO_LOAD = ["climate_ir"]

samsung_ns = cg.esphome_ns.namespace("samsung")
SamsungClimate = samsung_ns.class_("SamsungClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(SamsungClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

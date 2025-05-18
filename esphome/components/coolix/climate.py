import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@glmnet"]

coolix_ns = cg.esphome_ns.namespace("coolix")
CoolixClimate = coolix_ns.class_("CoolixClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(CoolixClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

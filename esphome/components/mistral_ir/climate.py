import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@lerhum"]

mistral_ir_ns = cg.esphome_ns.namespace("mistral_ir")
MistralIR = mistral_ir_ns.class_("MistralIR", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(MistralIR)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@fonix232"]

climate_ir_siemens_ira211_ns = cg.esphome_ns.namespace("climate_ir_siemens_ira211")
SiemensIRA211Climate = climate_ir_siemens_ira211_ns.class_(
    "SiemensIRA211Climate", climate_ir.ClimateIR
)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(SiemensIRA211Climate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

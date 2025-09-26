import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@cfeenstra1024"]

zhlt01_ns = cg.esphome_ns.namespace("zhlt01")
ZHLT01Climate = zhlt01_ns.class_("ZHLT01Climate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(ZHLT01Climate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

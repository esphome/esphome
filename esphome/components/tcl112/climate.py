import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@glmnet"]

tcl112_ns = cg.esphome_ns.namespace("tcl112")
Tcl112Climate = tcl112_ns.class_("Tcl112Climate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(Tcl112Climate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)

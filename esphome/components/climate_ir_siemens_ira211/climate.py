import esphome.codegen as cg
from esphome.components import climate_ir, remote_base
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir"]

climate_ir_siemens_ira211_ns = cg.esphome_ns.namespace("climate_ir_siemens_ira211")
SiemensIRA211Climate = climate_ir_siemens_ira211_ns.class_(
    "SiemensIRA211Climate", climate_ir.ClimateIR
)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(SiemensIRA211Climate)


async def to_code(config: ConfigType) -> None:
    remote_base.request_protocol("ira211")  # used from C++
    await climate_ir.new_climate_ir(config)

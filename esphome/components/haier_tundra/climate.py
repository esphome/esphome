import esphome.codegen as cg
from esphome.components import climate_ir, remote_base
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir"]

haier_tundra_ns = cg.esphome_ns.namespace("haier_tundra")
HaierTundra = haier_tundra_ns.class_("HaierTundra", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(HaierTundra)


async def to_code(config: ConfigType) -> None:
    remote_base.request_protocol("haier")
    await climate_ir.new_climate_ir(config)

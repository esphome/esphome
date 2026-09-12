from esphome.components import climate_ir
from esphome.types import ConfigType

from . import fujitsu_general_ns

AUTO_LOAD = ["climate_ir"]

FujitsuGeneralClimate = fujitsu_general_ns.class_(
    "FujitsuGeneralClimate", climate_ir.ClimateIR
)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(FujitsuGeneralClimate)


async def to_code(config: ConfigType) -> None:
    await climate_ir.new_climate_ir(config)

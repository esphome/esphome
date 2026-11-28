import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import fujitsu_general_ns

AUTO_LOAD = ["climate_ir"]

CONF_STATE_HEADER_BYTE = "state_header_byte"

FujitsuGeneralClimate = fujitsu_general_ns.class_(
    "FujitsuGeneralClimate", climate_ir.ClimateIR
)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(
    FujitsuGeneralClimate
).extend(
    {
        cv.Optional(CONF_STATE_HEADER_BYTE): cv.hex_uint8_t,
    }
)


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)
    cg.add_optional(var.set_state_header_byte, config.get(CONF_STATE_HEADER_BYTE))

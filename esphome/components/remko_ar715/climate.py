import esphome.codegen as cg
from esphome.components import climate_ir
from esphome.components.climate_ir import CONF_SUPPORTS_HEAT_COOL
import esphome.config_validation as cv
from esphome.const import CONF_SUPPORTS_HEAT
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir"]

remko_ar715_ns = cg.esphome_ns.namespace("remko_ar715")
RemkoAr715Climate = remko_ar715_ns.class_("RemkoAr715Climate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_schema(RemkoAr715Climate).extend(
    {
        cv.Optional(CONF_SUPPORTS_HEAT, default=False): cv.boolean_false,
        cv.Optional(CONF_SUPPORTS_HEAT_COOL, default=False): cv.boolean_false,
    }
)


async def to_code(config: ConfigType) -> None:
    await climate_ir.new_climate_ir(config)

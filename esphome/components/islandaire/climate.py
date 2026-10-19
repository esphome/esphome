import esphome.codegen as cg
from esphome.components import climate_ir
from esphome.components.climate_ir import CONF_SUPPORTS_HEAT_COOL
import esphome.config_validation as cv
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@ipopov"]

islandaire_ns = cg.esphome_ns.namespace("islandaire")
IslandaireClimate = islandaire_ns.class_("IslandaireClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(IslandaireClimate).extend(
    {
        # The unit has no auto (heat/cool) mode
        cv.Optional(CONF_SUPPORTS_HEAT_COOL, default=False): cv.boolean_false,
    }
)


async def to_code(config: ConfigType) -> None:
    await climate_ir.new_climate_ir(config)

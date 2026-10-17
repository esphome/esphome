import esphome.codegen as cg
from esphome.components import climate_ir
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@ipopov"]

islandaire_ns = cg.esphome_ns.namespace("islandaire")
IslandaireClimate = islandaire_ns.class_("IslandaireClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(IslandaireClimate)


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)
    # The unit has no auto (heat/cool) mode.
    cg.add(var.set_supports_heat_cool(False))

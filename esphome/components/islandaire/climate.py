import esphome.codegen as cg
from esphome.components import climate_ir
from esphome.components.climate_ir import CONF_SUPPORTS_HEAT_COOL
import esphome.config_validation as cv
from esphome.const import CONF_SENSOR, UNIT_FAHRENHEIT
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@ipopov"]

islandaire_ns = cg.esphome_ns.namespace("islandaire")
IslandaireClimate = islandaire_ns.class_("IslandaireClimate", climate_ir.ClimateIR)

# The unit reports in Fahrenheit (traits() in islandaire.cpp), so visual temperatures are kept in it
CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(
    IslandaireClimate, fixed_temperature_unit=UNIT_FAHRENHEIT
).extend(
    {
        # The unit has no auto (heat/cool) mode
        cv.Optional(CONF_SUPPORTS_HEAT_COOL, default=False): cv.boolean_false,
        # The unit works in Fahrenheit, so a Celsius temperature sensor cannot feed it
        cv.Optional(CONF_SENSOR): cv.invalid(
            "'sensor' is not supported by the Islandaire climate component"
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    await climate_ir.new_climate_ir(config)

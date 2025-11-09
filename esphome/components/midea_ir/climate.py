import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_USE_FAHRENHEIT

AUTO_LOAD = ["climate_ir", "coolix"]
CODEOWNERS = ["@dudanov"]

midea_ir_ns = cg.esphome_ns.namespace("midea_ir")
MideaIR = midea_ir_ns.class_("MideaIR", climate_ir.ClimateIR)


CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(MideaIR).extend(
    {
        cv.Optional(CONF_USE_FAHRENHEIT, default=False): cv.boolean,
    }
)


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_fahrenheit(config[CONF_USE_FAHRENHEIT]))

import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_USE_FAHRENHEIT

AUTO_LOAD = ["climate_ir"]

daikin_brc_ns = cg.esphome_ns.namespace("daikin_brc")
DaikinBrcClimate = daikin_brc_ns.class_("DaikinBrcClimate", climate_ir.ClimateIR)


CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(DaikinBrcClimate).extend(
    {
        cv.Optional(CONF_USE_FAHRENHEIT, default=False): cv.boolean,
    }
)


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_fahrenheit(config[CONF_USE_FAHRENHEIT]))

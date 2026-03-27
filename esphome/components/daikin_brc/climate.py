import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_USE_FAHRENHEIT

AUTO_LOAD = ["climate_ir"]

CONF_SUB_UNIT = "sub_unit"

daikin_brc_ns = cg.esphome_ns.namespace("daikin_brc")
DaikinBrcClimate = daikin_brc_ns.class_("DaikinBrcClimate", climate_ir.ClimateIR)


CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(DaikinBrcClimate).extend(
    {
        cv.Optional(CONF_USE_FAHRENHEIT, default=False): cv.boolean,
        cv.Optional(CONF_SUB_UNIT, default=0): cv.int_range(min=0, max=5),
    }
)


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_fahrenheit(config[CONF_USE_FAHRENHEIT]))
    cg.add(var.set_sub_unit(config[CONF_SUB_UNIT]))

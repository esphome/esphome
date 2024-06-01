import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import CONF_ID

AUTO_LOAD = ["climate_ir"]

ykh531e_ns = cg.esphome_ns.namespace("ykh531e")
YKH531EClimate = ykh531e_ns.class_("YKH531EClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {cv.GenerateID(): cv.declare_id(YKH531EClimate)}
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)

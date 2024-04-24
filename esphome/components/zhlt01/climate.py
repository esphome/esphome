import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import CONF_ID

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@cfeenstra1024"]

zhlt01_ns = cg.esphome_ns.namespace("zhlt01")
ZHLT01Climate = zhlt01_ns.class_("ZHLT01Climate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {cv.GenerateID(): cv.declare_id(ZHLT01Climate)}
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)

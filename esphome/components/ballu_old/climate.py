import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import CONF_ID

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@Midnighter32"]

ballu_old_ns = cg.esphome_ns.namespace("ballu_old")
BalluOldClimate = ballu_old_ns.class_("BalluOldClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(BalluOldClimate),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)

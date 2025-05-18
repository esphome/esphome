import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_MODEL

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@kbx81"]

toshiba_ns = cg.esphome_ns.namespace("toshiba")
ToshibaClimate = toshiba_ns.class_("ToshibaClimate", climate_ir.ClimateIR)

Model = toshiba_ns.enum("Model")
MODELS = {
    "GENERIC": Model.MODEL_GENERIC,
    "RAC-PT1411HWRU-C": Model.MODEL_RAC_PT1411HWRU_C,
    "RAC-PT1411HWRU-F": Model.MODEL_RAC_PT1411HWRU_F,
}

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(ToshibaClimate).extend(
    {
        cv.Optional(CONF_MODEL, default="generic"): cv.enum(MODELS, upper=True),
    }
)


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))

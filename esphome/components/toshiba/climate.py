import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import CONF_ID, CONF_MODEL

AUTO_LOAD = ["climate_ir"]

toshiba_ns = cg.esphome_ns.namespace("toshiba")
ToshibaClimate = toshiba_ns.class_("ToshibaClimate", climate_ir.ClimateIR)

Model = toshiba_ns.enum("Model")
MODELS = {
    "GENERIC": Model.MODEL_GENERIC,
    "WH-TA01LE": Model.MODEL_WH_TA01LE,
}

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ToshibaClimate),
        cv.Optional(CONF_MODEL, default="GENERIC"): cv.enum(MODELS, upper=True),
    }
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield climate_ir.register_climate_ir(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))

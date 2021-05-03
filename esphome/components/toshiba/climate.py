import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import CONF_ID, CONF_MODEL

AUTO_LOAD = ["climate_ir"]

toshiba_ns = cg.esphome_ns.namespace("toshiba")
ToshibaClimate = toshiba_ns.class_("ToshibaClimate", climate_ir.ClimateIR)

Model = toshiba_ns.enum("Model")
MODELS = {
    "WH-TA17NE": Model.MODEL_WH_TA17NE,
    "WH-TA01LE": Model.MODEL_WH_TA01LE,
    "WH-H01EE": Model.MODEL_WH_H01EE,
}

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(ToshibaClimate),
    cv.Required(CONF_MODEL): cv.enum(MODELS, upper=True),
})


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield climate_ir.register_climate_ir(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))

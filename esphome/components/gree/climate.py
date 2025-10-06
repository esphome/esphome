import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_MODEL

CODEOWNERS = ["@orestismers"]

AUTO_LOAD = ["climate_ir"]

gree_ns = cg.esphome_ns.namespace("gree")
GreeClimate = gree_ns.class_("GreeClimate", climate_ir.ClimateIR)

Model = gree_ns.enum("Model")
MODELS = {
    "generic": Model.GREE_GENERIC,
    "yan": Model.GREE_YAN,
    "yaa": Model.GREE_YAA,
    "yac": Model.GREE_YAC,
    "yac1fb9": Model.GREE_YAC1FB9,
    "yx1ff": Model.GREE_YX1FF,
    "yag": Model.GREE_YAG,
}

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(GreeClimate).extend(
    {
        cv.Required(CONF_MODEL): cv.enum(MODELS),
    }
)


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import (
    CONF_ID,
    CONF_MODEL,
    CONF_IFEEL
)

AUTO_LOAD = ["climate_ir"]

gree_ns = cg.esphome_ns.namespace("gree")
GreeClimate = gree_ns.class_("GreeClimate", climate_ir.ClimateIR)

Model = gree_ns.enum("Model")
MODELS = {
    "generic": Model.GREE_GENERIC,
    "yan": Model.GREE_YAN,
    "yaa": Model.GREE_YAA,
    "yac": Model.GREE_YAC,
}

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(GreeClimate),
        cv.Required(CONF_MODEL): cv.enum(MODELS),
        cv.Optional(CONF_IFEEL): cv.boolean()
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    if CONF_IFEEL not in config:
        config[CONF_IFEEL] = False

    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_iFeel(config[CONF_IFEEL]))

    await climate_ir.register_climate_ir(var, config)

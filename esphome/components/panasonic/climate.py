import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import CONF_ID

AUTO_LOAD = ["climate_ir"]

panasonic_ns = cg.esphome_ns.namespace("panasonic")
PanasonicClimate = panasonic_ns.class_("PanasonicClimate", climate_ir.ClimateIR)

SUPPORTS_HORIZONTAL_SWING = "supports_horizontal_swing"
CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PanasonicClimate),
        cv.Optional(SUPPORTS_HORIZONTAL_SWING, default=False): cv.boolean,
    }
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if SUPPORTS_HORIZONTAL_SWING in config and SUPPORTS_HORIZONTAL_SWING is True:
        cg.add_define("SUPPORTS_HORIZONTAL_SWING")
    yield climate_ir.register_climate_ir(var, config)

import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_MODEL
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@glmnet", "@arpena"]

whirlpool_ns = cg.esphome_ns.namespace("whirlpool")
WhirlpoolClimate = whirlpool_ns.class_("WhirlpoolClimate", climate_ir.ClimateIR)

Model = whirlpool_ns.enum("Model")
MODELS = {
    "DG11J1-3A": Model.MODEL_DG11J1_3A,
    "DG11J1-91": Model.MODEL_DG11J1_91,
    "HISENSE_ARG": Model.MODEL_HISENSE_ARG,
}

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(WhirlpoolClimate).extend(
    {
        cv.Optional(CONF_MODEL, default="DG11J1-3A"): cv.enum(MODELS, upper=True),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))
    # cv.enum keeps the YAML key as a str subclass; the MockObj value cannot be compared
    if config[CONF_MODEL] == "HISENSE_ARG":
        cg.add_define("USE_WHIRLPOOL_HISENSE_ARG")

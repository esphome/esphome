import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_MODEL
from esphome.types import ConfigType

CODEOWNERS = ["@duckle29"]
AUTO_LOAD = ["climate_ir"]

climate_ir_panasonic_ns = cg.esphome_ns.namespace("climate_ir_panasonic")
PanasonicIrClimate = climate_ir_panasonic_ns.class_(
    "PanasonicIrClimate", climate_ir.ClimateIR
)
Model = climate_ir_panasonic_ns.enum("Model", is_class=True)

MODELS = {
    "DKE": Model.DKE,
    "JKE": Model.JKE,
    "LKE": Model.LKE,
    "NKE": Model.NKE,
    "CKP": Model.CKP,
    "RKR": Model.RKR,
    "PKE": Model.PKE,
}

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(PanasonicIrClimate).extend(
    {
        cv.Optional(CONF_MODEL, default="JKE"): cv.enum(MODELS, upper=True),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))

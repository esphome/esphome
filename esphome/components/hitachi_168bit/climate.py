import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_MODEL

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@Rodrigoah"]

hitachi_ns = cg.esphome_ns.namespace("hitachi_168bit")
Hitachi168bitClimate = hitachi_ns.class_("Hitachi168bitClimate", climate_ir.ClimateIR)

Model = hitachi_ns.enum("Model")
MODELS = {
    "HCRA31NEWH": Model.MODEL_HCRA31NEWH,
}

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(Hitachi168bitClimate).extend(
    {
        cv.Optional(CONF_MODEL, default="HCRA31NEWH"): cv.enum(MODELS, upper=True),
    }
)


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))

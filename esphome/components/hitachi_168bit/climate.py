import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_MODEL
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@Rodrigoah"]

hitachi_168bit_ns = cg.esphome_ns.namespace("hitachi_168bit")
Hitachi168bitClimate = hitachi_168bit_ns.class_(
    "Hitachi168bitClimate", climate_ir.ClimateIR
)

Model = hitachi_168bit_ns.enum("Model")
MODELS = Model.options("HCRA31NEWH", prefix="MODEL_")

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(Hitachi168bitClimate).extend(
    {
        cv.Optional(CONF_MODEL, default="HCRA31NEWH"): cv.enum(MODELS, upper=True),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))

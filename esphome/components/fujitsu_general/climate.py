import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_MODEL
from esphome.types import ConfigType

from . import fujitsu_general_ns

AUTO_LOAD = ["climate_ir"]

FujitsuGeneralClimate = fujitsu_general_ns.class_(
    "FujitsuGeneralClimate", climate_ir.ClimateIR
)

Model = fujitsu_general_ns.enum("Model")
MODELS = Model.options("ARRAH2E", "ARREW4E", prefix="MODEL_")

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(
    FujitsuGeneralClimate
).extend(
    {
        cv.Optional(CONF_MODEL, default="arrah2e"): cv.enum(MODELS, upper=True),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))

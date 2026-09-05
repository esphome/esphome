import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_MODEL, CONF_USE_FAHRENHEIT

CODEOWNERS = ["@rwrozelle"]

AUTO_LOAD = ["climate_ir"]

friedrich_ns = cg.esphome_ns.namespace("friedrich")
FriedrichClimate = friedrich_ns.class_("FriedrichClimate", climate_ir.ClimateIR)

Model = friedrich_ns.enum("Model")
MODELS = {
    "MW12Y3H": Model.MODEL_MW12Y3H,
}

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(FriedrichClimate).extend(
    {
        cv.Optional(CONF_MODEL, default="MW12Y3H"): cv.enum(MODELS, upper=True),
        cv.Optional(CONF_USE_FAHRENHEIT, default=True): cv.boolean,
    }
)


def final_validate(config):
    if not config[CONF_USE_FAHRENHEIT]:
        raise cv.Invalid(
            "use_fahrenheit: false is not yet implemented for the Friedrich climate component"
        )


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_fahrenheit(config[CONF_USE_FAHRENHEIT]))

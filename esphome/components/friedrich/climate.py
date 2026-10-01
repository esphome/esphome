import esphome.codegen as cg
from esphome.components import climate_ir, remote_base
import esphome.config_validation as cv
from esphome.const import CONF_HUMIDITY_SENSOR, CONF_MODEL, CONF_SENSOR

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
    }
)


def final_validate(config):
    if CONF_SENSOR in config:
        raise cv.Invalid("'sensor' is not supported by the Friedrich climate component")
    if CONF_HUMIDITY_SENSOR in config:
        raise cv.Invalid(
            "'humidity_sensor' is not supported by the Friedrich climate component"
        )


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    remote_base.request_protocol("aeha")  # used from C++
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))

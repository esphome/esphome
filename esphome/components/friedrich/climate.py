import esphome.codegen as cg
from esphome.components import climate_ir, remote_base
import esphome.config_validation as cv
from esphome.const import CONF_MODEL, CONF_SENSOR
from esphome.types import ConfigType

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@rwrozelle"]

friedrich_ns = cg.esphome_ns.namespace("friedrich")
FriedrichClimate = friedrich_ns.class_("FriedrichClimate", climate_ir.ClimateIR)

Model = friedrich_ns.enum("Model", is_class=True)
MODELS = {
    "MW12Y3H": Model.MODEL_MW12Y3H,
}

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(FriedrichClimate).extend(
    {
        cv.Optional(CONF_MODEL, default="MW12Y3H"): cv.enum(MODELS, upper=True),
        # The unit works in Fahrenheit, so a Celsius temperature sensor cannot feed it
        cv.Optional(CONF_SENSOR): cv.invalid(
            "'sensor' is not supported by the Friedrich climate component"
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    remote_base.request_protocol("aeha")  # used from C++
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))

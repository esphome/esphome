import esphome.codegen as cg
from esphome.components import climate_ir, remote_base
import esphome.config_validation as cv
from esphome.const import CONF_MODEL, CONF_USE_FAHRENHEIT
from esphome.types import ConfigType

from . import gree_ns

CODEOWNERS = ["@orestismers"]

AUTO_LOAD = ["climate_ir"]
GreeClimate = gree_ns.class_("GreeClimate", climate_ir.ClimateIR)

Model = gree_ns.enum("Model")
MODELS = Model.options(
    "generic",
    "yan",
    "yaa",
    "yac",
    "yac1fb9",
    "yb1fa",
    "yx1ff",
    "yag",
    "yaw1f",
    prefix="GREE_",
)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(GreeClimate).extend(
    {
        cv.Required(CONF_MODEL): cv.enum(MODELS, lower=True),
        cv.Optional(CONF_USE_FAHRENHEIT, default=False): cv.boolean,
    }
)


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))
    if config[CONF_USE_FAHRENHEIT]:
        cg.add_define("USE_GREE_FAHRENHEIT")
        cg.add(var.set_use_fahrenheit(True))
    if config[CONF_MODEL] == "yaw1f":
        cg.add_define("USE_GREE_YAW1F")
    if remote_base.CONF_RECEIVER_ID in config:
        cg.add_define("USE_GREE_RECEIVER")

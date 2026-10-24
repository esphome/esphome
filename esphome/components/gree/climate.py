import esphome.codegen as cg
from esphome.components import climate_ir
from esphome.components.const import CONF_HORIZONTAL_DEFAULT, CONF_VERTICAL_DEFAULT
import esphome.config_validation as cv
from esphome.const import CONF_MODEL
from esphome.types import ConfigType

from . import gree_ns

CODEOWNERS = ["@orestismers"]

AUTO_LOAD = ["climate_ir"]
GreeClimate = gree_ns.class_("GreeClimate", climate_ir.ClimateIR)

Model = gree_ns.enum("Model")
MODELS = Model.options(
    "generic", "yan", "yaa", "yac", "yac1fb9", "yx1ff", "yag", "yap1f", prefix="GREE_"
)

# String allow-lists only: the C++ default-vane wiring lives in the direction-defaults
# change. YAP1F still rejects non-auto values because its transmit frame omits them.
HORIZONTAL_DIRECTIONS = ("auto", "left", "mleft", "middle", "mright", "right")
VERTICAL_DIRECTIONS = ("auto", "up", "mup", "middle", "mdown", "down")

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(GreeClimate).extend(
    {
        cv.Required(CONF_MODEL): cv.enum(MODELS, lower=True),
        cv.Optional(CONF_HORIZONTAL_DEFAULT, default="auto"): cv.one_of(
            *HORIZONTAL_DIRECTIONS, lower=True
        ),
        cv.Optional(CONF_VERTICAL_DEFAULT, default="auto"): cv.one_of(
            *VERTICAL_DIRECTIONS, lower=True
        ),
    }
)


def _validate_yap1f_direction_defaults(config: ConfigType) -> ConfigType:
    if str(config[CONF_MODEL]) == "yap1f" and (
        str(config[CONF_HORIZONTAL_DEFAULT]) != "auto"
        or str(config[CONF_VERTICAL_DEFAULT]) != "auto"
    ):
        raise cv.Invalid("YAP1F does not support configurable vane defaults")

    return config


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _validate_yap1f_direction_defaults)


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))

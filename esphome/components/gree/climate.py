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

HorizontalDirections = gree_ns.enum("HorizontalDirections")
HORIZONTAL_DIRECTIONS = HorizontalDirections.options(
    "auto", "left", "mleft", "middle", "mright", "right", prefix="HORIZONTAL_DIRECTION_"
)

VerticalDirections = gree_ns.enum("VerticalDirections")
VERTICAL_DIRECTIONS = VerticalDirections.options(
    "auto", "up", "mup", "middle", "mdown", "down", prefix="VERTICAL_DIRECTION_"
)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(GreeClimate).extend(
    {
        cv.Required(CONF_MODEL): cv.enum(MODELS, lower=True),
        cv.Optional(CONF_HORIZONTAL_DEFAULT, default="auto"): cv.enum(
            HORIZONTAL_DIRECTIONS, lower=True
        ),
        cv.Optional(CONF_VERTICAL_DEFAULT, default="auto"): cv.enum(
            VERTICAL_DIRECTIONS, lower=True
        ),
    }
)


def _validate_direction_defaults(config):
    if str(config[CONF_MODEL]) == "yap1f" and (
        str(config[CONF_HORIZONTAL_DEFAULT]) != "auto"
        or str(config[CONF_VERTICAL_DEFAULT]) != "auto"
    ):
        raise cv.Invalid("YAP1F does not support configurable vane defaults")

    return config


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _validate_direction_defaults)


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_horizontal_default(config[CONF_HORIZONTAL_DEFAULT]))
    cg.add(var.set_vertical_default(config[CONF_VERTICAL_DEFAULT]))

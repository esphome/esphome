import esphome.codegen as cg
from esphome.components import climate_ir
from esphome.components.const import CONF_HORIZONTAL_DEFAULT, CONF_VERTICAL_DEFAULT
import esphome.config_validation as cv
from esphome.const import CONF_MODEL

from . import gree_ns

CODEOWNERS = ["@orestismers"]

AUTO_LOAD = ["climate_ir"]
GreeClimate = gree_ns.class_("GreeClimate", climate_ir.ClimateIR)

Model = gree_ns.enum("Model")
MODELS = {
    "generic": Model.GREE_GENERIC,
    "yan": Model.GREE_YAN,
    "yaa": Model.GREE_YAA,
    "yac": Model.GREE_YAC,
    "yac1fb9": Model.GREE_YAC1FB9,
    "yx1ff": Model.GREE_YX1FF,
    "yag": Model.GREE_YAG,
    "yap1f": Model.GREE_YAP1F,
}

HorizontalDirections = gree_ns.enum("HorizontalDirections")
HORIZONTAL_DIRECTIONS = {
    "auto": HorizontalDirections.HORIZONTAL_DIRECTION_AUTO,
    "left": HorizontalDirections.HORIZONTAL_DIRECTION_LEFT,
    "mleft": HorizontalDirections.HORIZONTAL_DIRECTION_MLEFT,
    "middle": HorizontalDirections.HORIZONTAL_DIRECTION_MIDDLE,
    "mright": HorizontalDirections.HORIZONTAL_DIRECTION_MRIGHT,
    "right": HorizontalDirections.HORIZONTAL_DIRECTION_RIGHT,
}

VerticalDirections = gree_ns.enum("VerticalDirections")
VERTICAL_DIRECTIONS = {
    "auto": VerticalDirections.VERTICAL_DIRECTION_AUTO,
    "up": VerticalDirections.VERTICAL_DIRECTION_UP,
    "mup": VerticalDirections.VERTICAL_DIRECTION_MUP,
    "middle": VerticalDirections.VERTICAL_DIRECTION_MIDDLE,
    "mdown": VerticalDirections.VERTICAL_DIRECTION_MDOWN,
    "down": VerticalDirections.VERTICAL_DIRECTION_DOWN,
}

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


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_horizontal_default(config[CONF_HORIZONTAL_DEFAULT]))
    cg.add(var.set_vertical_default(config[CONF_VERTICAL_DEFAULT]))

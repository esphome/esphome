import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv

CODEOWNERS = ["@RubyBailey"]
AUTO_LOAD = ["climate_ir"]

mitsubishi_ns = cg.esphome_ns.namespace("mitsubishi")
MitsubishiClimate = mitsubishi_ns.class_("MitsubishiClimate", climate_ir.ClimateIR)

CONF_SET_FAN_MODE = "set_fan_mode"
SetFanMode = mitsubishi_ns.enum("SetFanMode")
SETFANMODE = {
    "quiet_4levels": SetFanMode.MITSUBISHI_FAN_Q4L,
    #    "5levels": SetFanMode.MITSUBISHI_FAN_5L,
    "4levels": SetFanMode.MITSUBISHI_FAN_4L,
    "3levels": SetFanMode.MITSUBISHI_FAN_3L,
}

CONF_SUPPORTS_DRY = "supports_dry"
CONF_SUPPORTS_FAN_ONLY = "supports_fan_only"

CONF_HORIZONTAL_DEFAULT = "horizontal_default"
HorizontalDirections = mitsubishi_ns.enum("HorizontalDirections")
HORIZONTAL_DIRECTIONS = {
    "left": HorizontalDirections.HORIZONTAL_DIRECTION_LEFT,
    "middle-left": HorizontalDirections.HORIZONTAL_DIRECTION_MIDDLE_LEFT,
    "middle": HorizontalDirections.HORIZONTAL_DIRECTION_MIDDLE,
    "middle-right": HorizontalDirections.HORIZONTAL_DIRECTION_MIDDLE_RIGHT,
    "right": HorizontalDirections.HORIZONTAL_DIRECTION_RIGHT,
    "split": HorizontalDirections.HORIZONTAL_DIRECTION_SPLIT,
}

CONF_VERTICAL_DEFAULT = "vertical_default"
VerticalDirections = mitsubishi_ns.enum("VerticalDirections")
VERTICAL_DIRECTIONS = {
    "auto": VerticalDirections.VERTICAL_DIRECTION_AUTO,
    "up": VerticalDirections.VERTICAL_DIRECTION_UP,
    "middle-up": VerticalDirections.VERTICAL_DIRECTION_MIDDLE_UP,
    "middle": VerticalDirections.VERTICAL_DIRECTION_MIDDLE,
    "middle-down": VerticalDirections.VERTICAL_DIRECTION_MIDDLE_DOWN,
    "down": VerticalDirections.VERTICAL_DIRECTION_DOWN,
}


CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(MitsubishiClimate).extend(
    {
        cv.Optional(CONF_SET_FAN_MODE, default="3levels"): cv.enum(SETFANMODE),
        cv.Optional(CONF_SUPPORTS_DRY, default=False): cv.boolean,
        cv.Optional(CONF_SUPPORTS_FAN_ONLY, default=False): cv.boolean,
        cv.Optional(CONF_HORIZONTAL_DEFAULT, default="middle"): cv.enum(
            HORIZONTAL_DIRECTIONS
        ),
        cv.Optional(CONF_VERTICAL_DEFAULT, default="middle"): cv.enum(
            VERTICAL_DIRECTIONS
        ),
    }
)


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)

    cg.add(var.set_fan_mode(config[CONF_SET_FAN_MODE]))
    cg.add(var.set_supports_dry(config[CONF_SUPPORTS_DRY]))
    cg.add(var.set_supports_fan_only(config[CONF_SUPPORTS_FAN_ONLY]))
    cg.add(var.set_horizontal_default(config[CONF_HORIZONTAL_DEFAULT]))
    cg.add(var.set_vertical_default(config[CONF_VERTICAL_DEFAULT]))

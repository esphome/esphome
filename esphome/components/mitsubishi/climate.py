import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import CONF_ID

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

CONF_SET_SUPPORTED_MODE = "set_supported_mode"
SetSupportedMode = mitsubishi_ns.enum("SetSupportedMode")
SETSUPPORTEDMODE = {
    "cool": SetSupportedMode.MITSUBISHI_OP_MODE_AC,
    "heat": SetSupportedMode.MITSUBISHI_OP_MODE_AH,
    "heat_cool": SetSupportedMode.MITSUBISHI_OP_MODE_AHC,
    "dry_heat_cool": SetSupportedMode.MITSUBISHI_OP_MODE_ADHC,
    "dry_fan_heat_cool": SetSupportedMode.MITSUBISHI_OP_MODE_ADFHC,
}


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


CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(MitsubishiClimate),
        cv.Optional(CONF_SET_FAN_MODE, default="3levels"): cv.enum(SETFANMODE),
        cv.Optional(CONF_SET_SUPPORTED_MODE, default="heat_cool"): cv.enum(
            SETSUPPORTEDMODE
        ),
        cv.Optional(CONF_HORIZONTAL_DEFAULT, default="middle"): cv.enum(
            HORIZONTAL_DIRECTIONS
        ),
        cv.Optional(CONF_VERTICAL_DEFAULT, default="middle"): cv.enum(
            VERTICAL_DIRECTIONS
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)

    if CONF_SET_FAN_MODE in config:
        cg.add(var.set_fan_mode(config[CONF_SET_FAN_MODE]))

    if CONF_SET_SUPPORTED_MODE in config:
        cg.add(var.set_supported_mode(config[CONF_SET_SUPPORTED_MODE]))

    if CONF_HORIZONTAL_DEFAULT in config:
        cg.add(var.set_horizontal_default(config[CONF_HORIZONTAL_DEFAULT]))

    if CONF_VERTICAL_DEFAULT in config:
        cg.add(var.set_vertical_default(config[CONF_VERTICAL_DEFAULT]))

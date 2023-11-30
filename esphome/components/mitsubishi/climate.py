import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import CONF_ID, CONF_LOW, CONF_MEDIUM, CONF_HIGH

CODEOWNERS = ["@RubyBailey"]
AUTO_LOAD = ["climate_ir"]

mitsubishi_ns = cg.esphome_ns.namespace("mitsubishi")
MitsubishiClimate = mitsubishi_ns.class_("MitsubishiClimate", climate_ir.ClimateIR)

CONF_SET_FAN_SPEEDS = "set_fan_speeds"
CONF_MEDIUM_LOW = "medium_low"

Setfanspeeds = mitsubishi_ns.enum("Setfanspeeds")
SETFANSPEEDS = {
    "fan1": Setfanspeeds.MITSUBISHI_FAN_1,
    "fan2": Setfanspeeds.MITSUBISHI_FAN_2,
    "fan3": Setfanspeeds.MITSUBISHI_FAN_3,
    "fan4": Setfanspeeds.MITSUBISHI_FAN_4,
    "fan5": Setfanspeeds.MITSUBISHI_FAN_5,
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
        cv.Optional(
            CONF_SET_FAN_SPEEDS,
            default={
                CONF_LOW: "fan1",
                CONF_MEDIUM: "fan2",
                CONF_HIGH: "fan3",
            },
        ): cv.All(
            cv.Schema(
                {
                    cv.Required(CONF_LOW): cv.enum(SETFANSPEEDS),
                    cv.Optional(CONF_MEDIUM_LOW): cv.enum(SETFANSPEEDS),                    
                    cv.Required(CONF_MEDIUM): cv.enum(SETFANSPEEDS),
                    cv.Required(CONF_HIGH): cv.enum(SETFANSPEEDS),
                }
            ),
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

    if CONF_SET_FAN_SPEEDS in config:
        fan = config[CONF_SET_FAN_SPEEDS]
        cg.add(var.set_fan_low(fan[CONF_LOW]))
        if CONF_MEDIUM_LOW in config:
            cg.add(var.set_fan_medium_low(fan[CONF_MEDIUM_LOW]))        
        cg.add(var.set_fan_medium(fan[CONF_MEDIUM]))
        cg.add(var.set_fan_hi(fan[CONF_HIGH]))

    if CONF_HORIZONTAL_DEFAULT in config:
        cg.add(var.set_horizontal_default(config[CONF_HORIZONTAL_DEFAULT]))

    if CONF_VERTICAL_DEFAULT in config:
        cg.add(var.set_vertical_default(config[CONF_VERTICAL_DEFAULT]))

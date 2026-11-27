import esphome.codegen as cg
from esphome.components import climate_ir
from esphome.components.const import CONF_HORIZONTAL_DEFAULT, CONF_VERTICAL_DEFAULT
import esphome.config_validation as cv
from esphome.types import ConfigType

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
CONF_FAHRENHEIT_COMPATIBILITY = "fahrenheit_compatibility"

HorizontalDirections = mitsubishi_ns.enum("HorizontalDirections")
HORIZONTAL_DIRECTIONS = HorizontalDirections.options(
    "left",
    "middle-left",
    "middle",
    "middle-right",
    "right",
    "split",
    prefix="HORIZONTAL_DIRECTION_",
)

VerticalDirections = mitsubishi_ns.enum("VerticalDirections")
VERTICAL_DIRECTIONS = VerticalDirections.options(
    "auto",
    "up",
    "middle-up",
    "middle",
    "middle-down",
    "down",
    prefix="VERTICAL_DIRECTION_",
)

CONF_VERTICAL_VANES = "vertical_vanes"


CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(MitsubishiClimate).extend(
    {
        cv.Optional(CONF_SET_FAN_MODE, default="3levels"): cv.enum(SETFANMODE),
        cv.Optional(CONF_SUPPORTS_DRY, default=False): cv.boolean,
        cv.Optional(CONF_SUPPORTS_FAN_ONLY, default=False): cv.boolean,
        cv.Optional(CONF_FAHRENHEIT_COMPATIBILITY, default=False): cv.boolean,
        cv.Optional(CONF_HORIZONTAL_DEFAULT, default="middle"): cv.enum(
            HORIZONTAL_DIRECTIONS
        ),
        cv.Optional(CONF_VERTICAL_DEFAULT, default="middle"): cv.enum(
            VERTICAL_DIRECTIONS
        ),
        cv.Optional(CONF_VERTICAL_VANES, default=1): cv.int_range(1, 2),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)

    cg.add(var.set_fan_mode(config[CONF_SET_FAN_MODE]))
    cg.add(var.set_supports_dry(config[CONF_SUPPORTS_DRY]))
    cg.add(var.set_supports_fan_only(config[CONF_SUPPORTS_FAN_ONLY]))
    cg.add(var.set_fahrenheit_compatibility(config[CONF_FAHRENHEIT_COMPATIBILITY]))
    cg.add(var.set_horizontal_default(config[CONF_HORIZONTAL_DEFAULT]))
    cg.add(var.set_vertical_default(config[CONF_VERTICAL_DEFAULT]))
    cg.add(var.set_vertical_vanes(config[CONF_VERTICAL_VANES]))

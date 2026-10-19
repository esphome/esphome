import esphome.codegen as cg
from esphome.components import light, output
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLUE,
    CONF_COLOR_INTERLOCK,
    CONF_GREEN,
    CONF_OUTPUT_ID,
    CONF_RED,
    CONF_WHITE,
)
from esphome.types import ConfigType

rgbw_ns = cg.esphome_ns.namespace("rgbw")
RGBWLightOutput = rgbw_ns.class_("RGBWLightOutput", light.LightOutput)

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(RGBWLightOutput),
        cv.Required(CONF_RED): cv.use_id(output.FloatOutput),
        cv.Required(CONF_GREEN): cv.use_id(output.FloatOutput),
        cv.Required(CONF_BLUE): cv.use_id(output.FloatOutput),
        cv.Required(CONF_WHITE): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_COLOR_INTERLOCK, default=False): cv.boolean,
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)

    await cg.add_variable(var.set_red, config[CONF_RED])
    await cg.add_variable(var.set_green, config[CONF_GREEN])
    await cg.add_variable(var.set_blue, config[CONF_BLUE])
    await cg.add_variable(var.set_white, config[CONF_WHITE])
    cg.add(var.set_color_interlock(config[CONF_COLOR_INTERLOCK]))

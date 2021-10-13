import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import (
    CONF_BLUE,
    CONF_COLOR_INTERLOCK,
    CONF_COLOR_TEMPERATURE,
    CONF_GREEN,
    CONF_RED,
    CONF_OUTPUT_ID,
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
)

CODEOWNERS = ["@jesserockz"]

rgbct_ns = cg.esphome_ns.namespace("rgbct")
RGBCTLightOutput = rgbct_ns.class_("RGBCTLightOutput", light.LightOutput)

CONF_WHITE_BRIGHTNESS = "white_brightness"

CONFIG_SCHEMA = cv.All(
    light.RGB_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(RGBCTLightOutput),
            cv.Required(CONF_RED): cv.use_id(output.FloatOutput),
            cv.Required(CONF_GREEN): cv.use_id(output.FloatOutput),
            cv.Required(CONF_BLUE): cv.use_id(output.FloatOutput),
            cv.Required(CONF_COLOR_TEMPERATURE): cv.use_id(output.FloatOutput),
            cv.Required(CONF_WHITE_BRIGHTNESS): cv.use_id(output.FloatOutput),
            cv.Required(CONF_COLD_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
            cv.Required(CONF_WARM_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
            cv.Optional(CONF_COLOR_INTERLOCK, default=False): cv.boolean,
        }
    ),
    light.validate_color_temperature_channels,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)

    red = await cg.get_variable(config[CONF_RED])
    cg.add(var.set_red(red))
    green = await cg.get_variable(config[CONF_GREEN])
    cg.add(var.set_green(green))
    blue = await cg.get_variable(config[CONF_BLUE])
    cg.add(var.set_blue(blue))

    color_temp = await cg.get_variable(config[CONF_COLOR_TEMPERATURE])
    cg.add(var.set_color_temperature(color_temp))
    white_brightness = await cg.get_variable(config[CONF_WHITE_BRIGHTNESS])
    cg.add(var.set_white_brightness(white_brightness))

    cg.add(var.set_cold_white_temperature(config[CONF_COLD_WHITE_COLOR_TEMPERATURE]))
    cg.add(var.set_warm_white_temperature(config[CONF_WARM_WHITE_COLOR_TEMPERATURE]))

    cg.add(var.set_color_interlock(config[CONF_COLOR_INTERLOCK]))

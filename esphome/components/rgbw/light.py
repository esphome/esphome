import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import (
    CONF_BLUE,
    CONF_COLOR_INTERLOCK,
    CONF_GREEN,
    CONF_RED,
    CONF_OUTPUT_ID,
    CONF_WHITE,
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
)

rgbw_ns = cg.esphome_ns.namespace("rgbw")
RGBWLightOutput = rgbw_ns.class_("RGBWLightOutput", light.LightOutput)
CONF_EMULATE_RGBWW = "emulate_rgbww"
CONF_ADDITIVE_BRIGHTNESS = "additive_brightness"


def validate_emulate_rgbww(value):
    emulate_blue = value.get(CONF_BLUE)
    emulate_red = value.get(CONF_RED)
    if emulate_red is None and emulate_blue is None:
        raise cv.Invalid(
            "If using emulate_rgbww, then must at least set either a blue or red percentage"
        )
    return value


CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(RGBWLightOutput),
        cv.Required(CONF_RED): cv.use_id(output.FloatOutput),
        cv.Required(CONF_GREEN): cv.use_id(output.FloatOutput),
        cv.Required(CONF_BLUE): cv.use_id(output.FloatOutput),
        cv.Required(CONF_WHITE): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_COLOR_INTERLOCK, default=False): cv.boolean,
        cv.Optional(CONF_EMULATE_RGBWW): cv.All(
            cv.Schema(
                {
                    cv.Required(
                        CONF_COLD_WHITE_COLOR_TEMPERATURE
                    ): cv.color_temperature,
                    cv.Required(
                        CONF_WARM_WHITE_COLOR_TEMPERATURE
                    ): cv.color_temperature,
                    cv.Optional(CONF_BLUE): cv.percentage,
                    cv.Optional(CONF_RED): cv.percentage,
                    cv.Optional(CONF_ADDITIVE_BRIGHTNESS, default=False): cv.boolean,
                }
            ),
            light.validate_color_temperature_channels,
            validate_emulate_rgbww,
        ),
    }
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
    white = await cg.get_variable(config[CONF_WHITE])
    cg.add(var.set_white(white))
    cg.add(var.set_color_interlock(config[CONF_COLOR_INTERLOCK]))
    if CONF_EMULATE_RGBWW in config:
        emulate_rgbww = config[CONF_EMULATE_RGBWW]
        cg.add(
            var.set_cold_white_temperature(
                emulate_rgbww[CONF_COLD_WHITE_COLOR_TEMPERATURE]
            )
        )
        cg.add(
            var.set_warm_white_temperature(
                emulate_rgbww[CONF_WARM_WHITE_COLOR_TEMPERATURE]
            )
        )
        if CONF_BLUE in emulate_rgbww:
            cg.add(var.set_blue_white_percentage(emulate_rgbww[CONF_BLUE]))
        if CONF_RED in emulate_rgbww:
            cg.add(var.set_red_white_percentage(emulate_rgbww[CONF_RED]))
        cg.add(var.set_additive_brightness(emulate_rgbww[CONF_ADDITIVE_BRIGHTNESS]))

import esphome.codegen as cg
from esphome.components import light, output
import esphome.config_validation as cv
from esphome.const import (
    CONF_BRIGHTNESS,
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_COLOR_TEMPERATURE,
    CONF_OUTPUT_ID,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
)
from esphome.types import ConfigType

CODEOWNERS = ["@jesserockz"]

color_temperature_ns = cg.esphome_ns.namespace("color_temperature")
CTLightOutput = color_temperature_ns.class_("CTLightOutput", light.LightOutput)

CONFIG_SCHEMA = cv.All(
    light.RGB_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(CTLightOutput),
            cv.Required(CONF_COLOR_TEMPERATURE): cv.use_id(output.FloatOutput),
            cv.Required(CONF_BRIGHTNESS): cv.use_id(output.FloatOutput),
            cv.Required(CONF_COLD_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
            cv.Required(CONF_WARM_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
        }
    ),
    light.validate_color_temperature_channels,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)

    await cg.add_variable(var.set_color_temperature, config[CONF_COLOR_TEMPERATURE])
    await cg.add_variable(var.set_brightness, config[CONF_BRIGHTNESS])

    cg.add(var.set_cold_white_temperature(config[CONF_COLD_WHITE_COLOR_TEMPERATURE]))
    cg.add(var.set_warm_white_temperature(config[CONF_WARM_WHITE_COLOR_TEMPERATURE]))

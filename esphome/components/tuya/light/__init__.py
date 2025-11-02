import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_COLOR_INTERLOCK,
    CONF_DEFAULT_TRANSITION_LENGTH,
    CONF_GAMMA_CORRECT,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_OUTPUT_ID,
    CONF_SWITCH_DATAPOINT,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
)

from .. import CONF_TUYA_ID, Tuya, tuya_ns

DEPENDENCIES = ["tuya"]

CONF_DIMMER_DATAPOINT = "dimmer_datapoint"
CONF_MIN_VALUE_DATAPOINT = "min_value_datapoint"
CONF_COLOR_TEMPERATURE_DATAPOINT = "color_temperature_datapoint"
CONF_COLOR_TEMPERATURE_INVERT = "color_temperature_invert"
CONF_COLOR_TEMPERATURE_MAX_VALUE = "color_temperature_max_value"
CONF_RGB_DATAPOINT = "rgb_datapoint"
CONF_HSV_DATAPOINT = "hsv_datapoint"
CONF_COLOR_DATAPOINT = "color_datapoint"
CONF_COLOR_TYPE = "color_type"

TuyaColorType = tuya_ns.enum("TuyaColorType")

COLOR_TYPES = {
    "RGB": TuyaColorType.RGB,
    "HSV": TuyaColorType.HSV,
    "RGBHSV": TuyaColorType.RGBHSV,
}

TuyaLight = tuya_ns.class_("TuyaLight", light.LightOutput, cg.Component)

COLOR_CONFIG_ERROR = (
    "This option has been removed, use color_datapoint and color_type instead."
)

CONFIG_SCHEMA = cv.All(
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TuyaLight),
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
            cv.Optional(CONF_DIMMER_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_MIN_VALUE_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_SWITCH_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_RGB_DATAPOINT): cv.invalid(COLOR_CONFIG_ERROR),
            cv.Optional(CONF_HSV_DATAPOINT): cv.invalid(COLOR_CONFIG_ERROR),
            cv.Inclusive(CONF_COLOR_DATAPOINT, "color"): cv.uint8_t,
            cv.Inclusive(CONF_COLOR_TYPE, "color"): cv.enum(COLOR_TYPES, upper=True),
            cv.Optional(CONF_COLOR_INTERLOCK, default=False): cv.boolean,
            cv.Inclusive(
                CONF_COLOR_TEMPERATURE_DATAPOINT, "color_temperature"
            ): cv.uint8_t,
            cv.Optional(CONF_COLOR_TEMPERATURE_INVERT, default=False): cv.boolean,
            cv.Optional(CONF_MIN_VALUE): cv.int_,
            cv.Optional(CONF_MAX_VALUE): cv.int_,
            cv.Optional(CONF_COLOR_TEMPERATURE_MAX_VALUE): cv.int_,
            cv.Inclusive(
                CONF_COLD_WHITE_COLOR_TEMPERATURE, "color_temperature"
            ): cv.color_temperature,
            cv.Inclusive(
                CONF_WARM_WHITE_COLOR_TEMPERATURE, "color_temperature"
            ): cv.color_temperature,
            # Change the default gamma_correct and default transition length settings.
            # The Tuya MCU handles transitions and gamma correction on its own.
            cv.Optional(CONF_GAMMA_CORRECT, default=1.0): cv.positive_float,
            cv.Optional(
                CONF_DEFAULT_TRANSITION_LENGTH, default="0s"
            ): cv.positive_time_period_milliseconds,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(
        CONF_DIMMER_DATAPOINT,
        CONF_SWITCH_DATAPOINT,
        CONF_COLOR_DATAPOINT,
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    if CONF_DIMMER_DATAPOINT in config:
        cg.add(var.set_dimmer_id(config[CONF_DIMMER_DATAPOINT]))
    if CONF_MIN_VALUE_DATAPOINT in config:
        cg.add(var.set_min_value_datapoint_id(config[CONF_MIN_VALUE_DATAPOINT]))
    if CONF_SWITCH_DATAPOINT in config:
        cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))
    if CONF_COLOR_DATAPOINT in config:
        cg.add(var.set_color_id(config[CONF_COLOR_DATAPOINT]))
        cg.add(var.set_color_type(config[CONF_COLOR_TYPE]))
    if CONF_COLOR_TEMPERATURE_DATAPOINT in config:
        cg.add(var.set_color_temperature_id(config[CONF_COLOR_TEMPERATURE_DATAPOINT]))
        cg.add(var.set_color_temperature_invert(config[CONF_COLOR_TEMPERATURE_INVERT]))

        cg.add(
            var.set_cold_white_temperature(config[CONF_COLD_WHITE_COLOR_TEMPERATURE])
        )
        cg.add(
            var.set_warm_white_temperature(config[CONF_WARM_WHITE_COLOR_TEMPERATURE])
        )
    if CONF_MIN_VALUE in config:
        cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
    if CONF_MAX_VALUE in config:
        cg.add(var.set_max_value(config[CONF_MAX_VALUE]))
    if CONF_COLOR_TEMPERATURE_MAX_VALUE in config:
        cg.add(
            var.set_color_temperature_max_value(
                config[CONF_COLOR_TEMPERATURE_MAX_VALUE]
            )
        )

    cg.add(var.set_color_interlock(config[CONF_COLOR_INTERLOCK]))
    paren = await cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))

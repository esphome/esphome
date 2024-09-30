import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import (
    CONF_BRIGHTNESS,
    CONF_CONTRAST,
    CONF_LAMBDA,
    CONF_RESET_PIN,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_TRANSFORM,
    CONF_INVERT_COLORS,
    CONF_MODEL,
)

CODEOWNERS = ["@latonita"]

st7567_base_ns = cg.esphome_ns.namespace("st7567_base")
ST7567 = st7567_base_ns.class_("ST7567", cg.PollingComponent, display.DisplayBuffer)

ST7567Model = st7567_base_ns.enum("ST7567Model", is_class=True)
MODELS = {
    "ST7567_128x64": ST7567Model.ST7567_128x64,
    "ST7570_128x128": ST7567Model.ST7570_128x128,
    "ST7570_102x102": ST7567Model.ST7570_102x102,
}

ST7567_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_INVERT_COLORS, default=False): cv.boolean,
        cv.Optional(CONF_TRANSFORM): cv.Schema(
            {
                cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
                cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
            }
        ),
        cv.Optional(CONF_MODEL, default="ST7567_128x64"): cv.enum(MODELS),
        cv.Optional(CONF_CONTRAST, default=35): cv.int_range(min=0, max=63),
        cv.Optional(CONF_BRIGHTNESS, default=3): cv.int_range(min=0, max=7),
    }
).extend(cv.polling_component_schema("1s"))


async def setup_st7567(var, config):
    await display.register_display(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))

    cg.add(var.set_invert_colors(config[CONF_INVERT_COLORS]))
    cg.add(var.set_contrast(config[CONF_CONTRAST]))
    cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))

    if CONF_TRANSFORM in config:
        transform = config[CONF_TRANSFORM]
        cg.add(var.set_mirror_x(transform[CONF_MIRROR_X]))
        cg.add(var.set_mirror_y(transform[CONF_MIRROR_Y]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

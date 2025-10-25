from esphome import pins
import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import (
    CONF_INVERT_COLORS,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_RESET_PIN,
    CONF_TRANSFORM,
)

CODEOWNERS = ["@latonita"]

st7567_base_ns = cg.esphome_ns.namespace("st7567_base")
ST7567 = st7567_base_ns.class_("ST7567", cg.PollingComponent, display.DisplayBuffer)
ST7567Model = st7567_base_ns.enum("ST7567Model")

# todo in future: reuse following constants from const.py when they are released


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
    }
).extend(cv.polling_component_schema("1s"))


async def setup_st7567(var, config):
    await display.register_display(var, config)

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))

    cg.add(var.init_invert_colors(config[CONF_INVERT_COLORS]))

    if CONF_TRANSFORM in config:
        transform = config[CONF_TRANSFORM]
        cg.add(var.init_mirror_x(transform[CONF_MIRROR_X]))
        cg.add(var.init_mirror_y(transform[CONF_MIRROR_Y]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

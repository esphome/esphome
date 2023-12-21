import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import (
    CONF_LAMBDA,
    CONF_RESET_PIN,
    CONF_INVERT,
)

CODEOWNERS = ["@latonita"]

st7567_base_ns = cg.esphome_ns.namespace("st7567_base")
ST7567 = st7567_base_ns.class_("ST7567", cg.PollingComponent, display.DisplayBuffer)
ST7567Model = st7567_base_ns.enum("ST7567Model")

# to reuse MIRROR constants from const.py when they are released
CONF_MIRROR_X = "mirror_x"
CONF_MIRROR_Y = "mirror_y"

ST7567_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
        cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
        cv.Optional(CONF_INVERT, default=False): cv.boolean,
    }
).extend(cv.polling_component_schema("1s"))


async def setup_st7567(var, config):
    await display.register_display(var, config)

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_MIRROR_X in config:
        cg.add(var.init_mirror_x(config[CONF_MIRROR_X]))
    if CONF_MIRROR_Y in config:
        cg.add(var.init_mirror_y(config[CONF_MIRROR_Y]))
    if CONF_INVERT in config:
        cg.add(var.init_invert(config[CONF_INVERT]))
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

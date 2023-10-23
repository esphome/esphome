import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import (
    CONF_EXTERNAL_VCC,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_RESET_PIN,
    CONF_BRIGHTNESS,
    CONF_CONTRAST,
    CONF_INVERT,
)

ssd1306_base_ns = cg.esphome_ns.namespace("ssd1306_base")
SSD1306 = ssd1306_base_ns.class_("SSD1306", cg.PollingComponent, display.DisplayBuffer)
SSD1306Model = ssd1306_base_ns.enum("SSD1306Model")

CONF_FLIP_X = "flip_x"
CONF_FLIP_Y = "flip_y"
CONF_OFFSET_X = "offset_x"
CONF_OFFSET_Y = "offset_y"

MODELS = {
    "SSD1306_128X32": SSD1306Model.SSD1306_MODEL_128_32,
    "SSD1306_128X64": SSD1306Model.SSD1306_MODEL_128_64,
    "SSD1306_96X16": SSD1306Model.SSD1306_MODEL_96_16,
    "SSD1306_64X48": SSD1306Model.SSD1306_MODEL_64_48,
    "SSD1306_64X32": SSD1306Model.SSD1306_MODEL_64_32,
    "SSD1306_72X40": SSD1306Model.SSD1306_MODEL_72_40,
    "SH1106_128X32": SSD1306Model.SH1106_MODEL_128_32,
    "SH1106_128X64": SSD1306Model.SH1106_MODEL_128_64,
    "SH1106_96X16": SSD1306Model.SH1106_MODEL_96_16,
    "SH1106_64X48": SSD1306Model.SH1106_MODEL_64_48,
    "SH1107_128X64": SSD1306Model.SH1107_MODEL_128_64,
    "SSD1305_128X32": SSD1306Model.SSD1305_MODEL_128_32,
    "SSD1305_128X64": SSD1306Model.SSD1305_MODEL_128_64,
}

SSD1306_MODEL = cv.enum(MODELS, upper=True, space="_")


def _validate(value):
    model = value[CONF_MODEL]
    if model not in ("SSD1305_128X32", "SSD1305_128X64"):
        # Contrast is default value (1.0) while brightness is not
        # Indicates user is using old `brightness` option
        if value[CONF_BRIGHTNESS] != 1.0 and value[CONF_CONTRAST] == 1.0:
            raise cv.Invalid(
                "SSD1306/SH1106 no longer accepts brightness option, "
                'please use "contrast" instead.'
            )

    return value


SSD1306_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.Required(CONF_MODEL): SSD1306_MODEL,
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.percentage,
        cv.Optional(CONF_CONTRAST, default=1.0): cv.percentage,
        cv.Optional(CONF_EXTERNAL_VCC): cv.boolean,
        cv.Optional(CONF_FLIP_X, default=True): cv.boolean,
        cv.Optional(CONF_FLIP_Y, default=True): cv.boolean,
        cv.Optional(CONF_OFFSET_X, default=0): cv.int_range(min=-32, max=32),
        cv.Optional(CONF_OFFSET_Y, default=0): cv.int_range(min=-32, max=32),
        cv.Optional(CONF_INVERT, default=False): cv.boolean,
    }
).extend(cv.polling_component_schema("1s"))


async def setup_ssd1306(var, config):
    await cg.register_component(var, config)
    await display.register_display(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))
    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_BRIGHTNESS in config:
        cg.add(var.init_brightness(config[CONF_BRIGHTNESS]))
    if CONF_CONTRAST in config:
        cg.add(var.init_contrast(config[CONF_CONTRAST]))
    if CONF_EXTERNAL_VCC in config:
        cg.add(var.set_external_vcc(config[CONF_EXTERNAL_VCC]))
    if CONF_FLIP_X in config:
        cg.add(var.init_flip_x(config[CONF_FLIP_X]))
    if CONF_FLIP_Y in config:
        cg.add(var.init_flip_y(config[CONF_FLIP_Y]))
    if CONF_OFFSET_X in config:
        cg.add(var.init_offset_x(config[CONF_OFFSET_X]))
    if CONF_OFFSET_Y in config:
        cg.add(var.init_offset_y(config[CONF_OFFSET_Y]))
    if CONF_INVERT in config:
        cg.add(var.init_invert(config[CONF_INVERT]))
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

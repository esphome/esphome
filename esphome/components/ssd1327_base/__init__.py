import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import CONF_BRIGHTNESS, CONF_LAMBDA, CONF_MODEL, CONF_RESET_PIN
from esphome.core import coroutine

CODEOWNERS = ["@kbx81"]

ssd1327_base_ns = cg.esphome_ns.namespace("ssd1327_base")
SSD1327 = ssd1327_base_ns.class_("SSD1327", cg.PollingComponent, display.DisplayBuffer)
SSD1327Model = ssd1327_base_ns.enum("SSD1327Model")

MODELS = {
    "SSD1327_128X128": SSD1327Model.SSD1327_MODEL_128_128,
}

SSD1327_MODEL = cv.enum(MODELS, upper=True, space="_")

SSD1327_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.Required(CONF_MODEL): SSD1327_MODEL,
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.percentage,
    }
).extend(cv.polling_component_schema("1s"))


@coroutine
def setup_ssd1327(var, config):
    yield cg.register_component(var, config)
    yield display.register_display(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))
    if CONF_RESET_PIN in config:
        reset = yield cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_BRIGHTNESS in config:
        cg.add(var.init_brightness(config[CONF_BRIGHTNESS]))
    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

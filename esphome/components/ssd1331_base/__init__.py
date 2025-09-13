from esphome import pins
import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import CONF_BRIGHTNESS, CONF_LAMBDA, CONF_RESET_PIN

CODEOWNERS = ["@kbx81"]

ssd1331_base_ns = cg.esphome_ns.namespace("ssd1331_base")
SSD1331 = ssd1331_base_ns.class_("SSD1331", cg.PollingComponent, display.DisplayBuffer)

SSD1331_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.percentage,
    }
).extend(cv.polling_component_schema("1s"))


async def setup_ssd1331(var, config):
    await display.register_display(var, config)

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_BRIGHTNESS in config:
        cg.add(var.init_brightness(config[CONF_BRIGHTNESS]))
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

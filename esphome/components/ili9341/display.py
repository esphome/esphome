import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
from esphome.const import (
    CONF_DC_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_PAGES,
    CONF_RESET_PIN,
)

DEPENDENCIES = ["spi"]

CONF_LED_PIN = "led_pin"

ili9341_ns = cg.esphome_ns.namespace("ili9341")
ili9341 = ili9341_ns.class_(
    "ILI9341Display", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)
ILI9341M5Stack = ili9341_ns.class_("ILI9341M5Stack", ili9341)
ILI9341TFT24 = ili9341_ns.class_("ILI9341TFT24", ili9341)

ILI9341Model = ili9341_ns.enum("ILI9341Model")

MODELS = {
    "M5STACK": ILI9341Model.M5STACK,
    "TFT_2.4": ILI9341Model.TFT_24,
}

ILI9341_MODEL = cv.enum(MODELS, upper=True, space="_")

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ili9341),
            cv.Required(CONF_MODEL): ILI9341_MODEL,
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_LED_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema(False)),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    if config[CONF_MODEL] == "M5STACK":
        lcd_type = ILI9341M5Stack
    if config[CONF_MODEL] == "TFT_2.4":
        lcd_type = ILI9341TFT24
    rhs = lcd_type.new()
    var = cg.Pvariable(config[CONF_ID], rhs)

    await cg.register_component(var, config)
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))
    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_LED_PIN in config:
        led_pin = await cg.gpio_pin_expression(config[CONF_LED_PIN])
        cg.add(var.set_led_pin(led_pin))

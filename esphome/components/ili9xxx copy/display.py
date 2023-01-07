import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
from esphome.const import (
    CONF_COLOR_PALETTE,
    CONF_DC_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_RAW_DATA_ID,
    CONF_PAGES,
    CONF_RESET_PIN,
    CONF_BACKLIGHT_PIN,
)
from esphome.core import HexInt

DEPENDENCIES = ["spi"]

CODEOWNERS = ["@nielsnl68"]

ili9XXX_ns = cg.esphome_ns.namespace("ili9xxx")
ili9XXXSPI = ili9XXX_ns.class_(
    "ILI9XXXDisplay", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)

ILI9XXXColorMode = ili9XXX_ns.enum("ILI9XXXColorMode")

MODELS = {
    "M5STACK": ili9XXX_ns.class_("ILI9XXXM5Stack", ili9XXXSPI),
    "TFT_2.4": ili9XXX_ns.class_("ILI9XXXILI9341", ili9XXXSPI),
    "TFT_2.4R": ili9XXX_ns.class_("ILI9XXXILI9342", ili9XXXSPI),
    "ILI9341": ili9XXX_ns.class_("ILI9XXXILI9341", ili9XXXSPI),
    "ILI9342": ili9XXX_ns.class_("ILI9XXXILI9342", ili9XXXSPI),
    #    "ILI9481": ili9XXX_ns.class_("ILI9XXXILI9481", ili9XXXSPI),
    #    "ILI9486": ili9XXX_ns.class_("ILI9XXXILI9486", ili9XXXSPI),
    "ILI9488": ili9XXX_ns.class_("ILI9XXXILI9488", ili9XXXSPI),
    "ST7796": ili9XXX_ns.class_("ILI9XXXST7796", ili9XXXSPI),
}

COLOR_PALETTE = cv.one_of("NONE", "GRAYSCALE")

CONF_LED_PIN = "led_pin"


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ili9XXXSPI),
            cv.Required(CONF_MODEL): cv.enum(MODELS, upper=True, space="_"),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_LED_PIN): cv.invalid(
                "The led_pin is renamed to :" + CONF_BACKLIGHT_PIN
            ),
            cv.Optional(CONF_BACKLIGHT_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_COLOR_PALETTE, default="NONE"): COLOR_PALETTE,
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema(False)),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    rhs = MODELS[config[CONF_MODEL]].new()
    var = cg.Pvariable(config[CONF_ID], rhs)

    await cg.register_component(var, config)
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)
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

    if CONF_BACKLIGHT_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_BACKLIGHT_PIN])
        cg.add(var.set_backlight_pin(reset))

    if config[CONF_COLOR_PALETTE] == "GRAYSCALE":
        cg.add(var.set_buffer_color_mode(ILI9XXXColorMode.BITS_8_INDEXED))
        rhs = []
        for x in range(256):
            rhs.extend([HexInt(x), HexInt(x), HexInt(x)])
        prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
        cg.add(var.set_palette(prog_arr))
    else:
        pass

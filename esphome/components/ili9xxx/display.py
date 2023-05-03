import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core, pins
from esphome.components import display, spi
from esphome.core import CORE, HexInt
from esphome.const import (
    CONF_COLOR_PALETTE,
    CONF_DC_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_RAW_DATA_ID,
    CONF_PAGES,
    CONF_RESET_PIN,
    CONF_DIMENSIONS,
)

DEPENDENCIES = ["spi"]


def AUTO_LOAD():
    if CORE.is_esp32:
        return ["psram"]
    return []


CODEOWNERS = ["@nielsnl68"]

ili9XXX_ns = cg.esphome_ns.namespace("ili9xxx")
ili9XXXSPI = ili9XXX_ns.class_(
    "ILI9XXXDisplay", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)

ILI9XXXColorMode = ili9XXX_ns.enum("ILI9XXXColorMode")

MODELS = {
    "M5STACK": ili9XXX_ns.class_("ILI9XXXM5Stack", ili9XXXSPI),
    "M5CORE": ili9XXX_ns.class_("ILI9XXXM5CORE", ili9XXXSPI),
    "TFT_2.4": ili9XXX_ns.class_("ILI9XXXILI9341", ili9XXXSPI),
    "TFT_2.4R": ili9XXX_ns.class_("ILI9XXXILI9342", ili9XXXSPI),
    "ILI9341": ili9XXX_ns.class_("ILI9XXXILI9341", ili9XXXSPI),
    "ILI9342": ili9XXX_ns.class_("ILI9XXXILI9342", ili9XXXSPI),
    "ILI9481": ili9XXX_ns.class_("ILI9XXXILI9481", ili9XXXSPI),
    "ILI9486": ili9XXX_ns.class_("ILI9XXXILI9486", ili9XXXSPI),
    "ILI9488": ili9XXX_ns.class_("ILI9XXXILI9488", ili9XXXSPI),
    "ST7796": ili9XXX_ns.class_("ILI9XXXST7796", ili9XXXSPI),
}

COLOR_PALETTE = cv.one_of("NONE", "GRAYSCALE", "IMAGE_ADAPTIVE")

CONF_LED_PIN = "led_pin"
CONF_COLOR_PALETTE_IMAGES = "color_palette_images"


def _validate(config):
    if config.get(CONF_COLOR_PALETTE) == "IMAGE_ADAPTIVE" and not config.get(
        CONF_COLOR_PALETTE_IMAGES
    ):
        raise cv.Invalid(
            "Color palette in IMAGE_ADAPTIVE mode requires at least one 'color_palette_images' entry to generate palette"
        )
    if (
        config.get(CONF_COLOR_PALETTE_IMAGES)
        and config.get(CONF_COLOR_PALETTE) != "IMAGE_ADAPTIVE"
    ):
        raise cv.Invalid(
            "Providing color palette images requires palette mode to be 'IMAGE_ADAPTIVE'"
        )
    if CORE.is_esp8266 and config.get(CONF_MODEL) not in [
        "M5STACK",
        "TFT_2.4",
        "TFT_2.4R",
        "ILI9341",
        "ILI9342",
    ]:
        raise cv.Invalid(
            "Provided model can't run on ESP8266. Use an ESP32 with PSRAM onboard"
        )
    return config


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ili9XXXSPI),
            cv.Required(CONF_MODEL): cv.enum(MODELS, upper=True, space="_"),
            cv.Optional(CONF_DIMENSIONS): cv.dimensions,
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_LED_PIN): cv.invalid(
                "This property is removed. To use the backlight use proper light component."
            ),
            cv.Optional(CONF_COLOR_PALETTE, default="NONE"): COLOR_PALETTE,
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
            cv.Optional(CONF_COLOR_PALETTE_IMAGES, default=[]): cv.ensure_list(
                cv.file_
            ),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema(False)),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
    _validate,
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

    if CONF_DIMENSIONS in config:
        cg.add(
            var.set_dimentions(config[CONF_DIMENSIONS][0], config[CONF_DIMENSIONS][1])
        )

    rhs = None
    if config[CONF_COLOR_PALETTE] == "GRAYSCALE":
        cg.add(var.set_buffer_color_mode(ILI9XXXColorMode.BITS_8_INDEXED))
        rhs = []
        for x in range(256):
            rhs.extend([HexInt(x), HexInt(x), HexInt(x)])
        prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
        cg.add(var.set_palette(prog_arr))
    elif config[CONF_COLOR_PALETTE] == "IMAGE_ADAPTIVE":
        cg.add(var.set_buffer_color_mode(ILI9XXXColorMode.BITS_8_INDEXED))
        from PIL import Image

        def load_image(filename):
            path = CORE.relative_config_path(filename)
            try:
                return Image.open(path)
            except Exception as e:
                raise core.EsphomeError(f"Could not load image file {path}: {e}")

        # make a wide horizontal combined image.
        images = [load_image(x) for x in config[CONF_COLOR_PALETTE_IMAGES]]
        total_width = sum(i.width for i in images)
        max_height = max(i.height for i in images)

        ref_image = Image.new("RGB", (total_width, max_height))
        x = 0
        for i in images:
            ref_image.paste(i, (x, 0))
            x = x + i.width

        # reduce the colors on combined image to 256.
        converted = ref_image.convert("P", palette=Image.ADAPTIVE, colors=256)
        # if you want to verify how the images look use
        # ref_image.save("ref_in.png")
        # converted.save("ref_out.png")
        palette = converted.getpalette()
        assert len(palette) == 256 * 3
        rhs = palette
    else:
        cg.add(var.set_buffer_color_mode(ILI9XXXColorMode.BITS_16))

    if rhs is not None:
        prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
        cg.add(var.set_palette(prog_arr))

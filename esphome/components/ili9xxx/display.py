import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core, pins
from esphome.components import display, spi, font
from esphome.components.display import validate_rotation
from esphome.core import CORE, HexInt
from esphome.const import (
    CONF_COLOR_PALETTE,
    CONF_DC_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_PAGES,
    CONF_RESET_PIN,
    CONF_DIMENSIONS,
    CONF_WIDTH,
    CONF_HEIGHT,
    CONF_ROTATION,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_SWAP_XY,
    CONF_COLOR_ORDER,
    CONF_OFFSET_HEIGHT,
    CONF_OFFSET_WIDTH,
    CONF_TRANSFORM,
    CONF_INVERT_COLORS,
    CONF_RED,
    CONF_GREEN,
    CONF_BLUE,
    CONF_DATA_PINS,
    CONF_TYPE,
    CONF_NUMBER,
    CONF_IGNORE_STRAPPING_WARNING,
)

DEPENDENCIES = ["spi"]


def AUTO_LOAD():
    if CORE.is_esp32:
        return ["psram"]
    return []


CODEOWNERS = ["@nielsnl68", "@clydebarrow"]

ili9xxx_ns = cg.esphome_ns.namespace("ili9xxx")
ILI9XXXDisplay = ili9xxx_ns.class_(
    "ILI9XXXDisplay",
    cg.PollingComponent,
    spi.SPIDevice,
    display.Display,
    display.DisplayBuffer,
)


displayInterface = ili9xxx_ns.class_("displayInterface")
SPI_Interface = ili9xxx_ns.class_("SPIBus", displayInterface)
SPI16D_Interface = ili9xxx_ns.class_("SPI16DBus", displayInterface)
RGB_Interface = ili9xxx_ns.class_("RGBBus", SPI_Interface)


ILI9XXXColorMode = ili9xxx_ns.enum("ILI9XXXColorMode")
ColorOrder = display.display_ns.enum("ColorMode")

MODELS = {
    "GC9A01A": ili9xxx_ns.class_("ILI9XXXGC9A01A", ILI9XXXDisplay),
    "M5STACK": ili9xxx_ns.class_("ILI9XXXM5Stack", ILI9XXXDisplay),
    "M5CORE": ili9xxx_ns.class_("ILI9XXXM5CORE", ILI9XXXDisplay),
    "TFT_2.4": ili9xxx_ns.class_("ILI9XXXILI9341", ILI9XXXDisplay),
    "TFT_2.4R": ili9xxx_ns.class_("ILI9XXXILI9342", ILI9XXXDisplay),
    "ILI9341": ili9xxx_ns.class_("ILI9XXXILI9341", ILI9XXXDisplay),
    "ILI9342": ili9xxx_ns.class_("ILI9XXXILI9342", ILI9XXXDisplay),
    "ILI9481": ili9xxx_ns.class_("ILI9XXXILI9481", ILI9XXXDisplay),
    "ILI9481-18": ili9xxx_ns.class_("ILI9XXXILI948118", ILI9XXXDisplay),
    "ILI9486": ili9xxx_ns.class_("ILI9XXXILI9486", ILI9XXXDisplay),
    "ILI9488": ili9xxx_ns.class_("ILI9XXXILI9488", ILI9XXXDisplay),
    "ILI9488_A": ili9xxx_ns.class_("ILI9XXXILI9488A", ILI9XXXDisplay),
    "ST7796": ili9xxx_ns.class_("ILI9XXXST7796", ILI9XXXDisplay),
    "ST7789V": ili9xxx_ns.class_("ILI9XXXST7789V", ILI9XXXDisplay),
    "S3BOX": ili9xxx_ns.class_("ILI9XXXS3Box", ILI9XXXDisplay),
    "S3BOX_LITE": ili9xxx_ns.class_("ILI9XXXS3BoxLite", ILI9XXXDisplay),
    "WAVESHARE_RES_3_5": ili9xxx_ns.class_("WAVESHARERES35", ILI9XXXDisplay),
}

COLOR_ORDERS = {
    "RGB": ColorOrder.COLOR_ORDER_RGB,
    "BGR": ColorOrder.COLOR_ORDER_BGR,
}

COLOR_PALETTE = cv.one_of("NONE", "GRAYSCALE", "IMAGE_ADAPTIVE")
CONF_BUS_ID = "bus_id"
CONF_DE_PIN = "de_pin"

CONF_COLOR_PALETTE_IMAGES = "color_palette_images"
CONF_COLOR_PALETTE_ID = "color_palette_id"

CONF_INTERFACE = "interface"

CONF_PCLK_PIN = "pclk_pin"
CONF_PCLK_FREQUENCY = "pclk_frequency"
CONF_PCLK_INVERTED = "pclk_inverted"

CONF_HSYNC_PIN = "hsync_pin"
CONF_HSYNC_PULSE_WIDTH = "hsync_pulse_width"
CONF_HSYNC_FRONT_PORCH = "hsync_front_porch"
CONF_HSYNC_BACK_PORCH = "hsync_back_porch"

CONF_VSYNC_PIN = "vsync_pin"
CONF_VSYNC_PULSE_WIDTH = "vsync_pulse_width"
CONF_VSYNC_FRONT_PORCH = "vsync_front_porch"
CONF_VSYNC_BACK_PORCH = "vsync_back_porch"

CONF_DISPLAY_COLORFORMAT = "disply_colorformat"
CONF_BUFFER_COLORFORMAT = "buffer_colorformat"
CONF_COLOR_MODE = "color_mode"
CONF_BYTE_ALIGNED = "byte_aligned"
CONF_LITTLE_ENDIAN = "little_endian"


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
        "ST7789V",
    ]:
        raise cv.Invalid(
            "Provided model can't run on ESP8266. Use an ESP32 with PSRAM onboard"
        )
    return config


DATA_PIN_SCHEMA = pins.internal_gpio_input_pin_schema


def data_pin_validate(value):
    """
    It is safe to use strapping pins as RGB output data bits, as they are outputs only,
    and not initialised until after boot.
    """
    if not isinstance(value, dict):
        try:
            return DATA_PIN_SCHEMA(
                {CONF_NUMBER: value, CONF_IGNORE_STRAPPING_WARNING: True}
            )
        except cv.Invalid:
            pass
    return DATA_PIN_SCHEMA(value)


def data_pin_set(length):
    return cv.All(
        [data_pin_validate],
        cv.Length(min=length, max=length, msg=f"Exactly {length} data pins required"),
    )


DPI_INTERFACE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DATA_PINS): cv.Any(
            data_pin_set(16),
            cv.Schema(
                {
                    cv.Required(CONF_RED): data_pin_set(5),
                    cv.Required(CONF_GREEN): data_pin_set(6),
                    cv.Required(CONF_BLUE): data_pin_set(5),
                }
            ),
        ),
        cv.Required(CONF_DE_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_PCLK_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_PCLK_INVERTED, default=True): cv.boolean,
        cv.Optional(CONF_PCLK_FREQUENCY, default="16MHz"): cv.All(
            cv.frequency, cv.Range(min=4e6, max=30e6)
        ),
        cv.Required(CONF_HSYNC_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_HSYNC_PULSE_WIDTH, default=10): cv.int_,
        cv.Optional(CONF_HSYNC_BACK_PORCH, default=10): cv.int_,
        cv.Optional(CONF_HSYNC_FRONT_PORCH, default=20): cv.int_,
        cv.Required(CONF_VSYNC_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_VSYNC_PULSE_WIDTH, default=10): cv.int_,
        cv.Optional(CONF_VSYNC_BACK_PORCH, default=10): cv.int_,
        cv.Optional(CONF_VSYNC_FRONT_PORCH, default=10): cv.int_,
    },
)

COMMON_SCHEMA = (
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ILI9XXXDisplay),
            cv.Required(CONF_MODEL): cv.enum(MODELS, upper=True, space="_"),
            cv.Optional(CONF_DIMENSIONS): cv.Any(
                cv.dimensions,
                cv.Schema(
                    {
                        cv.Required(CONF_WIDTH): cv.int_,
                        cv.Required(CONF_HEIGHT): cv.int_,
                        cv.Optional(CONF_OFFSET_HEIGHT, default=0): cv.int_,
                        cv.Optional(CONF_OFFSET_WIDTH, default=0): cv.int_,
                    }
                ),
            ),
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_COLOR_PALETTE, default="NONE"): COLOR_PALETTE,
            cv.GenerateID(CONF_COLOR_PALETTE_ID): cv.declare_id(cg.uint8),
            cv.Optional(CONF_COLOR_PALETTE_IMAGES, default=[]): cv.ensure_list(
                cv.file_
            ),
            cv.Optional(CONF_INVERT_COLORS): cv.boolean,
            cv.Optional(CONF_COLOR_ORDER): cv.one_of(*COLOR_ORDERS.keys(), upper=True),
            cv.Exclusive(CONF_ROTATION, CONF_ROTATION): validate_rotation,
            cv.Exclusive(CONF_TRANSFORM, CONF_ROTATION): cv.Schema(
                {
                    cv.Optional(CONF_SWAP_XY, default=False): cv.boolean,
                    cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
                    cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
                }
            ),
        },
    ).extend(cv.polling_component_schema("1s"))
)


CONFIG_SCHEMA = cv.All(
    font.validate_pillow_installed,
    cv.typed_schema(
        {
            "SPI": spi.spi_device_schema(False, "40MHz")
            .extend(
                {
                    cv.GenerateID(CONF_BUS_ID): cv.declare_id(SPI_Interface),
                    cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
                }
            )
            .extend(COMMON_SCHEMA),
            "SPI16D": spi.spi_device_schema(False, "40MHz")
            .extend(
                {
                    cv.GenerateID(CONF_BUS_ID): cv.declare_id(SPI16D_Interface),
                    cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
                }
            )
            .extend(COMMON_SCHEMA),
            "DPI_RGB": DPI_INTERFACE_SCHEMA.extend(COMMON_SCHEMA),
            "SPI_RGB": DPI_INTERFACE_SCHEMA.extend(COMMON_SCHEMA)
            .extend(
                {
                    cv.Optional(CONF_DC_PIN): pins.gpio_output_pin_schema,
                }
            )
            .extend(spi.spi_device_schema(cs_pin_required=False, default_data_rate=1e6)),
        },
        default_type="SPI",
        key=CONF_INTERFACE,
        upper=True,
    ),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
    _validate,
)


async def register_display_iobus(var, config):
    bus_config = config[CONF_INTERFACE]
    bus = cg.new_Pvariable(bus_config[CONF_BUS_ID])
    cg.add(var.set_interface(bus))

    if bus_config[CONF_TYPE] == "SPI":
        await spi.register_spi_device(bus, bus_config)
        dc = await cg.gpio_pin_expression(bus_config[CONF_DC_PIN])
        cg.add(bus.set_dc_pin(dc))

    elif bus_config[CONF_TYPE] == "SPI16D":
        await spi.register_spi_device(bus, bus_config)
        dc = await cg.gpio_pin_expression(bus_config[CONF_DC_PIN])
        cg.add(bus.set_dc_pin(dc))

    elif bus_config[CONF_TYPE] == "DPI_RGB":
        pin = await cg.gpio_pin_expression(config[CONF_HSYNC_PIN])
        cg.add(bus.set_hsync_pin(pin))
        cg.add(bus.set_hsync_pulse_width(config[CONF_HSYNC_PULSE_WIDTH]))
        cg.add(bus.set_hsync_back_porch(config[CONF_HSYNC_BACK_PORCH]))
        cg.add(bus.set_hsync_front_porch(config[CONF_HSYNC_FRONT_PORCH]))

        pin = await cg.gpio_pin_expression(config[CONF_VSYNC_PIN])
        cg.add(bus.set_vsync_pin(pin))
        cg.add(bus.set_vsync_pulse_width(config[CONF_VSYNC_PULSE_WIDTH]))
        cg.add(bus.set_vsync_back_porch(config[CONF_VSYNC_BACK_PORCH]))
        cg.add(bus.set_vsync_front_porch(config[CONF_VSYNC_FRONT_PORCH]))

        pin = await cg.gpio_pin_expression(config[CONF_PCLK_PIN])
        cg.add(bus.set_pclk_pin(pin))
        cg.add(bus.set_pclk_inverted(config[CONF_PCLK_INVERTED]))
        cg.add(bus.set_pclk_frequency(config[CONF_PCLK_FREQUENCY]))

        index = 0
        for pin in config[CONF_DATA_PINS]:
            data_pin = await cg.gpio_pin_expression(pin)
            cg.add(bus.add_data_pin(data_pin, index))
            index += 1

        pin = await cg.gpio_pin_expression(config[CONF_DE_PIN])
        cg.add(bus.set_de_pin(pin))


async def to_code(config):
    rhs = MODELS[config[CONF_MODEL]].new()
    var = cg.Pvariable(config[CONF_ID], rhs)

    await display.register_display(var, config)

    await register_display_iobus(var, config)

    if CONF_COLOR_ORDER in config:
        cg.add(var.set_color_order(COLOR_ORDERS[config[CONF_COLOR_ORDER]]))
    if CONF_TRANSFORM in config:
        transform = config[CONF_TRANSFORM]
        cg.add(var.set_swap_xy(transform[CONF_SWAP_XY]))
        cg.add(var.set_mirror_x(transform[CONF_MIRROR_X]))
        cg.add(var.set_mirror_y(transform[CONF_MIRROR_Y]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))

    if CONF_DIMENSIONS in config:
        dimensions = config[CONF_DIMENSIONS]
        if isinstance(dimensions, dict):
            cg.add(var.set_dimensions(dimensions[CONF_WIDTH], dimensions[CONF_HEIGHT]))
            cg.add(
                var.set_offsets(
                    dimensions[CONF_OFFSET_WIDTH], dimensions[CONF_OFFSET_HEIGHT]
                )
            )
        else:
            (width, height) = dimensions
            cg.add(var.set_dimensions(width, height))

    rhs = None
    if config[CONF_COLOR_PALETTE] == "GRAYSCALE":
        cg.add(var.set_buffer_color_mode(ILI9XXXColorMode.BITS_8_INDEXED))
        rhs = []
        for x in range(256):
            rhs.extend([HexInt(x), HexInt(x), HexInt(x)])
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
        converted = ref_image.convert("P", palette=Image.Palette.ADAPTIVE, colors=256)
        # if you want to verify how the images look use
        # ref_image.save("ref_in.png")
        # converted.save("ref_out.png")
        palette = converted.getpalette()
        assert len(palette) == 256 * 3
        rhs = palette
    else:
        cg.add(var.set_buffer_color_mode(ILI9XXXColorMode.BITS_16))

    if rhs is not None:
        prog_arr = cg.progmem_array(config[CONF_COLOR_PALETTE_ID], rhs)
        cg.add(var.set_palette(prog_arr))

    if CONF_INVERT_COLORS in config:
        cg.add(var.invert_colors(config[CONF_INVERT_COLORS]))

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi, display
from esphome.const import (
    CONF_DC_PIN,
    CONF_RESET_PIN,
    CONF_OUTPUT,
    CONF_DATA_PINS,
    CONF_ID,
    CONF_DIMENSIONS,
    CONF_WIDTH,
    CONF_HEIGHT,
    CONF_OFFSET_HEIGHT,
    CONF_OFFSET_WIDTH,
)

from .init_sequences import ST7701S_INITS

CONF_MIRROR_X = "mirror_x"
CONF_MIRROR_Y = "mirror_y"
CONF_SWAP_XY = "swap_xy"
CONF_COLOR_ORDER = "color_order"
CONF_TRANSFORM = "transform"
CONF_INIT_SEQUENCE = "init_sequence"
CONF_DE_PIN = "de_pin"
CONF_PCLK_PIN = "pclk_pin"
CONF_HSYNC_PIN = "hsync_pin"
CONF_VSYNC_PIN = "vsync_pin"

CONF_HSYNC_PULSE_WIDTH = "hsync_pulse_width"
CONF_HSYNC_BACK_PORCH = "hsync_back_porch"
CONF_HSYNC_FRONT_PORCH = "hsync_front_porch"
CONF_VSYNC_PULSE_WIDTH = "vsync_pulse_width"
CONF_VSYNC_BACK_PORCH = "vsync_back_porch"
CONF_VSYNC_FRONT_PORCH = "vsync_front_porch"
CONF_INVERT_COLORS = "invert_colors"

DEPENDENCIES = ["spi"]

st7701s_ns = cg.esphome_ns.namespace("st7701s")
ST7701S = st7701s_ns.class_("ST7701S", display.Display, cg.Component, spi.SPIDevice)
ColorOrder = display.display_ns.enum("ColorMode")

COLOR_ORDERS = {
    "RGB": ColorOrder.COLOR_MODE_RGB,
    "BGR": ColorOrder.COLOR_MODE_BGR,
}
DATA_PIN_SCHEMA = pins.gpio_pin_schema(
    {
        CONF_OUTPUT: True,
    },
    internal=True,
)


def map_sequence(value):
    """
    An initialisation sequence can be selected from one of the pre-defined sequences in init_sequences.py,
    or can be a literal array of data bytes.
    The format is a repeated sequence of [CMD, LEN, <data>] where <data> is LEN bytes.
    """
    if not isinstance(value, list):
        value = cv.int_(value)
        value = cv.one_of(*ST7701S_INITS)(value)
        value = ST7701S_INITS[value]
    value = cv.ensure_list(cv.uint8_t)(value)
    data_length = len(value)
    i = 0
    while i < data_length:
        remaining = data_length - i
        # Command byte is at value[i], length of data at value[i+1]
        if remaining < 2 or value[i + 1] > remaining - 2:
            raise cv.Invalid(f"Malformed initialisation sequence at index {i}")
        i += 2 + value[i + 1]
    return value


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(ST7701S),
                cv.Required(CONF_DIMENSIONS): cv.Any(
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
                cv.Optional(CONF_TRANSFORM): cv.Schema(
                    {
                        cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
                        cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
                    }
                ),
                cv.Required(CONF_DATA_PINS): cv.All(
                    [DATA_PIN_SCHEMA],
                    cv.Length(min=16, max=16, msg="Exactly 16 data pins required"),
                ),
                cv.Optional(CONF_INIT_SEQUENCE, default=1): map_sequence,
                cv.Optional(CONF_COLOR_ORDER): cv.one_of(
                    *COLOR_ORDERS.keys(), upper=True
                ),
                cv.Optional(CONF_INVERT_COLORS, default=False): cv.boolean,
                cv.Required(CONF_DE_PIN): pins.internal_gpio_output_pin_schema,
                cv.Required(CONF_PCLK_PIN): pins.internal_gpio_output_pin_schema,
                cv.Required(CONF_HSYNC_PIN): pins.internal_gpio_output_pin_schema,
                cv.Required(CONF_VSYNC_PIN): pins.internal_gpio_output_pin_schema,
                cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
                cv.Optional(CONF_DC_PIN): pins.gpio_output_pin_schema,
                cv.Optional(CONF_HSYNC_PULSE_WIDTH, default=10): cv.int_,
                cv.Optional(CONF_HSYNC_BACK_PORCH, default=10): cv.int_,
                cv.Optional(CONF_HSYNC_FRONT_PORCH, default=20): cv.int_,
                cv.Optional(CONF_VSYNC_PULSE_WIDTH, default=10): cv.int_,
                cv.Optional(CONF_VSYNC_BACK_PORCH, default=10): cv.int_,
                cv.Optional(CONF_VSYNC_FRONT_PORCH, default=10): cv.int_,
            }
        ).extend(spi.spi_device_schema(cs_pin_required=False, default_data_rate=1e6))
    ),
    cv.only_with_esp_idf,
)


async def to_code(config):
    var = config[CONF_ID]
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_color_mode(COLOR_ORDERS[config[CONF_COLOR_ORDER]]))
    cg.add(var.set_invert_colors(config[CONF_INVERT_COLORS]))
    cg.add(var.set_init_sequence(config[CONF_INIT_SEQUENCE]))
    cg.add(var.set_hsync_pulse_width(config[CONF_HSYNC_PULSE_WIDTH]))
    cg.add(var.set_hsync_back_porch(config[CONF_HSYNC_BACK_PORCH]))
    cg.add(var.set_hsync_front_porch(config[CONF_HSYNC_FRONT_PORCH]))
    cg.add(var.set_vsync_pulse_width(config[CONF_VSYNC_PULSE_WIDTH]))
    cg.add(var.set_vsync_back_porch(config[CONF_VSYNC_BACK_PORCH]))
    cg.add(var.set_vsync_front_porch(config[CONF_VSYNC_FRONT_PORCH]))
    index = 0
    for pin in config[CONF_DATA_PINS]:
        data_pin = await cg.gpio_pin_expression(pin)
        cg.add(var.add_data_pin(data_pin, index))
        index += 1

    if dc_pin := config.get(CONF_DC_PIN):
        dc = await cg.gpio_pin_expression(dc_pin)
        cg.add(var.set_dc_pin(dc))

    if reset_pin := config.get(CONF_RESET_PIN):
        reset = await cg.gpio_pin_expression(reset_pin)
        cg.add(var.set_reset_pin(reset))

    if transform := config.get(CONF_TRANSFORM):
        cg.add(var.set_mirror_x(transform[CONF_MIRROR_X]))
        cg.add(var.set_mirror_y(transform[CONF_MIRROR_Y]))

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

    pin = await cg.gpio_pin_expression(config[CONF_DE_PIN])
    cg.add(var.set_de_pin(pin))
    pin = await cg.gpio_pin_expression(config[CONF_PCLK_PIN])
    cg.add(var.set_pclk_pin(pin))
    pin = await cg.gpio_pin_expression(config[CONF_HSYNC_PIN])
    cg.add(var.set_hsync_pin(pin))
    pin = await cg.gpio_pin_expression(config[CONF_VSYNC_PIN])
    cg.add(var.set_vsync_pin(pin))

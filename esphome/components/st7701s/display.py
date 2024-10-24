from esphome import pins
import esphome.codegen as cg
from esphome.components import display, spi
from esphome.components.esp32 import const, only_on_variant
from esphome.components.rpi_dpi_rgb.display import (
    CONF_PCLK_FREQUENCY,
    CONF_PCLK_INVERTED,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLUE,
    CONF_COLOR_ORDER,
    CONF_DATA_PINS,
    CONF_DC_PIN,
    CONF_DIMENSIONS,
    CONF_GREEN,
    CONF_HEIGHT,
    CONF_HSYNC_PIN,
    CONF_ID,
    CONF_IGNORE_STRAPPING_WARNING,
    CONF_INIT_SEQUENCE,
    CONF_INVERT_COLORS,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_NUMBER,
    CONF_OFFSET_HEIGHT,
    CONF_OFFSET_WIDTH,
    CONF_RED,
    CONF_RESET_PIN,
    CONF_TRANSFORM,
    CONF_VSYNC_PIN,
    CONF_WIDTH,
)
from esphome.core import TimePeriod

from .init_sequences import ST7701S_INITS, cmd

CONF_DE_PIN = "de_pin"
CONF_PCLK_PIN = "pclk_pin"

CONF_HSYNC_PULSE_WIDTH = "hsync_pulse_width"
CONF_HSYNC_BACK_PORCH = "hsync_back_porch"
CONF_HSYNC_FRONT_PORCH = "hsync_front_porch"
CONF_VSYNC_PULSE_WIDTH = "vsync_pulse_width"
CONF_VSYNC_BACK_PORCH = "vsync_back_porch"
CONF_VSYNC_FRONT_PORCH = "vsync_front_porch"

DEPENDENCIES = ["spi", "esp32"]

st7701s_ns = cg.esphome_ns.namespace("st7701s")
ST7701S = st7701s_ns.class_("ST7701S", display.Display, cg.Component, spi.SPIDevice)
ColorOrder = display.display_ns.enum("ColorMode")
ST7701S_DELAY_FLAG = 0xFF

COLOR_ORDERS = {
    "RGB": ColorOrder.COLOR_ORDER_RGB,
    "BGR": ColorOrder.COLOR_ORDER_BGR,
}
DATA_PIN_SCHEMA = pins.internal_gpio_output_pin_schema


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


def map_sequence(value):
    """
    An initialisation sequence can be selected from one of the pre-defined sequences in init_sequences.py,
    or can be a literal array of data bytes.
    The format is a repeated sequence of [CMD, <data>] where <data> is s a sequence of bytes. The length is inferred
    from the length of the sequence and should not be explicit.
    A delay can be inserted by specifying "- delay N" where N is in ms
    """
    if isinstance(value, str) and value.lower().startswith("delay "):
        value = value.lower()[6:]
        delay = cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(TimePeriod(milliseconds=1), TimePeriod(milliseconds=255)),
        )(value)
        return [delay, ST7701S_DELAY_FLAG]
    if not isinstance(value, list):
        value = cv.int_(value)
        value = cv.one_of(*ST7701S_INITS)(value)
        return ST7701S_INITS[value]
    value = cv.Length(min=1, max=254)(value)
    return cmd(*value)


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
                cv.Optional(CONF_INIT_SEQUENCE, default=1): cv.ensure_list(
                    map_sequence
                ),
                cv.Optional(CONF_COLOR_ORDER): cv.one_of(
                    *COLOR_ORDERS.keys(), upper=True
                ),
                cv.Optional(CONF_PCLK_FREQUENCY, default="16MHz"): cv.All(
                    cv.frequency, cv.Range(min=4e6, max=30e6)
                ),
                cv.Optional(CONF_PCLK_INVERTED, default=True): cv.boolean,
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
    only_on_variant(supported=[const.VARIANT_ESP32S3]),
    cv.only_with_esp_idf,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    sequence = []
    for seq in config[CONF_INIT_SEQUENCE]:
        sequence.extend(seq)
    cg.add(var.set_init_sequence(sequence))
    cg.add(var.set_color_mode(COLOR_ORDERS[config[CONF_COLOR_ORDER]]))
    cg.add(var.set_invert_colors(config[CONF_INVERT_COLORS]))
    cg.add(var.set_hsync_pulse_width(config[CONF_HSYNC_PULSE_WIDTH]))
    cg.add(var.set_hsync_back_porch(config[CONF_HSYNC_BACK_PORCH]))
    cg.add(var.set_hsync_front_porch(config[CONF_HSYNC_FRONT_PORCH]))
    cg.add(var.set_vsync_pulse_width(config[CONF_VSYNC_PULSE_WIDTH]))
    cg.add(var.set_vsync_back_porch(config[CONF_VSYNC_BACK_PORCH]))
    cg.add(var.set_vsync_front_porch(config[CONF_VSYNC_FRONT_PORCH]))
    cg.add(var.set_pclk_inverted(config[CONF_PCLK_INVERTED]))
    cg.add(var.set_pclk_frequency(config[CONF_PCLK_FREQUENCY]))
    index = 0
    dpins = []
    if CONF_RED in config[CONF_DATA_PINS]:
        red_pins = config[CONF_DATA_PINS][CONF_RED]
        green_pins = config[CONF_DATA_PINS][CONF_GREEN]
        blue_pins = config[CONF_DATA_PINS][CONF_BLUE]
        if config[CONF_COLOR_ORDER] == "BGR":
            dpins.extend(red_pins)
            dpins.extend(green_pins)
            dpins.extend(blue_pins)
        else:
            dpins.extend(blue_pins)
            dpins.extend(green_pins)
            dpins.extend(red_pins)
        # swap bytes to match big-endian format
        dpins = dpins[8:16] + dpins[0:8]
    else:
        dpins = config[CONF_DATA_PINS]
    for pin in dpins:
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

    if lamb := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lamb, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    pin = await cg.gpio_pin_expression(config[CONF_DE_PIN])
    cg.add(var.set_de_pin(pin))
    pin = await cg.gpio_pin_expression(config[CONF_PCLK_PIN])
    cg.add(var.set_pclk_pin(pin))
    pin = await cg.gpio_pin_expression(config[CONF_HSYNC_PIN])
    cg.add(var.set_hsync_pin(pin))
    pin = await cg.gpio_pin_expression(config[CONF_VSYNC_PIN])
    cg.add(var.set_vsync_pin(pin))

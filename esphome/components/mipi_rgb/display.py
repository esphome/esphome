import importlib
import pkgutil

from esphome import pins
import esphome.codegen as cg
from esphome.components import display, spi
from esphome.components.const import (
    BYTE_ORDER_BIG,
    BYTE_ORDER_LITTLE,
    CONF_BYTE_ORDER,
    CONF_DRAW_ROUNDING,
)
from esphome.components.display import CONF_SHOW_TEST_CARD
from esphome.components.esp32 import VARIANT_ESP32S3, only_on_variant
from esphome.components.mipi import (
    COLOR_ORDERS,
    CONF_DE_PIN,
    CONF_HSYNC_BACK_PORCH,
    CONF_HSYNC_FRONT_PORCH,
    CONF_HSYNC_PULSE_WIDTH,
    CONF_PCLK_PIN,
    CONF_PIXEL_MODE,
    CONF_USE_AXIS_FLIPS,
    CONF_VSYNC_BACK_PORCH,
    CONF_VSYNC_FRONT_PORCH,
    CONF_VSYNC_PULSE_WIDTH,
    MODE_RGB,
    PIXEL_MODE_16BIT,
    PIXEL_MODE_18BIT,
    DriverChip,
    dimension_schema,
    map_sequence,
    power_of_two,
    requires_buffer,
)
from esphome.components.rpi_dpi_rgb.display import (
    CONF_PCLK_FREQUENCY,
    CONF_PCLK_INVERTED,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLUE,
    CONF_COLOR_ORDER,
    CONF_CS_PIN,
    CONF_DATA_PINS,
    CONF_DATA_RATE,
    CONF_DC_PIN,
    CONF_DIMENSIONS,
    CONF_DISABLED,
    CONF_ENABLE_PIN,
    CONF_GREEN,
    CONF_HSYNC_PIN,
    CONF_ID,
    CONF_IGNORE_STRAPPING_WARNING,
    CONF_INIT_SEQUENCE,
    CONF_INVERT_COLORS,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODEL,
    CONF_NUMBER,
    CONF_RED,
    CONF_RESET_PIN,
    CONF_ROTATION,
    CONF_SPI_ID,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
    CONF_VSYNC_PIN,
    CONF_WIDTH,
)
from esphome.final_validate import full_config

from ..spi import CONF_SPI_MODE, SPI_DATA_RATE_SCHEMA, SPI_MODE_OPTIONS, SPIComponent
from . import models

DEPENDENCIES = ["esp32", "psram"]

mipi_rgb_ns = cg.esphome_ns.namespace("mipi_rgb")
mipi_rgb = mipi_rgb_ns.class_("MipiRgb", display.Display, cg.Component)
mipi_rgb_spi = mipi_rgb_ns.class_(
    "MipiRgbSpi", mipi_rgb, display.Display, cg.Component, spi.SPIDevice
)
ColorOrder = display.display_ns.enum("ColorMode")

DATA_PIN_SCHEMA = pins.internal_gpio_output_pin_schema

DriverChip("CUSTOM")

# Import all models dynamically from the models package

for module_info in pkgutil.iter_modules(models.__path__):
    importlib.import_module(f".models.{module_info.name}", package=__package__)

MODELS = DriverChip.get_models()


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


def model_schema(config):
    model = MODELS[config[CONF_MODEL].upper()]
    transform = cv.Any(
        cv.Schema(
            {
                cv.Required(CONF_MIRROR_X): cv.boolean,
                cv.Required(CONF_MIRROR_Y): cv.boolean,
                **model.swap_xy_schema(),
            }
        ),
        cv.one_of(CONF_DISABLED, lower=True),
    )
    # RPI model does not use an init sequence, indicates with empty list
    if model.initsequence is None:
        # Custom model requires an init sequence
        iseqconf = cv.Required(CONF_INIT_SEQUENCE)
        uses_spi = True
    else:
        iseqconf = cv.Optional(CONF_INIT_SEQUENCE)
        uses_spi = CONF_INIT_SEQUENCE in config or len(model.initsequence) != 0
    # Dimensions are optional if the model has a default width and the x-y transform is not overridden
    transform_config = config.get(CONF_TRANSFORM, {})
    is_swapped = (
        isinstance(transform_config, dict)
        and transform_config.get(CONF_SWAP_XY, False) is True
    )
    cv_dimensions = (
        cv.Optional if model.get_default(CONF_WIDTH) and not is_swapped else cv.Required
    )

    pixel_modes = (PIXEL_MODE_16BIT, PIXEL_MODE_18BIT, "16", "18")
    schema = display.FULL_DISPLAY_SCHEMA.extend(
        {
            model.option(CONF_RESET_PIN, cv.UNDEFINED): pins.gpio_output_pin_schema,
            cv.GenerateID(): cv.declare_id(mipi_rgb_spi if uses_spi else mipi_rgb),
            cv_dimensions(CONF_DIMENSIONS): dimension_schema(
                model.get_default(CONF_DRAW_ROUNDING, 1)
            ),
            model.option(CONF_ENABLE_PIN, cv.UNDEFINED): cv.ensure_list(
                pins.gpio_output_pin_schema
            ),
            model.option(CONF_COLOR_ORDER, MODE_RGB): cv.enum(COLOR_ORDERS, upper=True),
            model.option(CONF_DRAW_ROUNDING, 2): power_of_two,
            model.option(CONF_PIXEL_MODE, PIXEL_MODE_16BIT): cv.one_of(
                *pixel_modes, lower=True
            ),
            cv.Optional(CONF_TRANSFORM): transform,
            cv.Required(CONF_MODEL): cv.one_of(model.name, upper=True),
            model.option(CONF_INVERT_COLORS, False): cv.boolean,
            model.option(CONF_USE_AXIS_FLIPS, True): cv.boolean,
            model.option(CONF_PCLK_FREQUENCY, "40MHz"): cv.All(
                cv.frequency, cv.Range(min=4e6, max=100e6)
            ),
            model.option(CONF_PCLK_INVERTED, True): cv.boolean,
            iseqconf: cv.ensure_list(map_sequence),
            model.option(CONF_BYTE_ORDER, BYTE_ORDER_BIG): cv.one_of(
                BYTE_ORDER_LITTLE, BYTE_ORDER_BIG, lower=True
            ),
            model.option(CONF_HSYNC_PULSE_WIDTH): cv.int_,
            model.option(CONF_HSYNC_BACK_PORCH): cv.int_,
            model.option(CONF_HSYNC_FRONT_PORCH): cv.int_,
            model.option(CONF_VSYNC_PULSE_WIDTH): cv.int_,
            model.option(CONF_VSYNC_BACK_PORCH): cv.int_,
            model.option(CONF_VSYNC_FRONT_PORCH): cv.int_,
            model.option(CONF_DATA_PINS): cv.Any(
                data_pin_set(16),
                cv.Schema(
                    {
                        cv.Required(CONF_RED): data_pin_set(5),
                        cv.Required(CONF_GREEN): data_pin_set(6),
                        cv.Required(CONF_BLUE): data_pin_set(5),
                    }
                ),
            ),
            model.option(
                CONF_DE_PIN, cv.UNDEFINED
            ): pins.internal_gpio_output_pin_schema,
            model.option(CONF_PCLK_PIN): pins.internal_gpio_output_pin_schema,
            model.option(CONF_HSYNC_PIN): pins.internal_gpio_output_pin_schema,
            model.option(CONF_VSYNC_PIN): pins.internal_gpio_output_pin_schema,
            model.option(CONF_RESET_PIN, cv.UNDEFINED): pins.gpio_output_pin_schema,
        }
    )
    if uses_spi:
        schema = schema.extend(
            {
                cv.GenerateID(CONF_SPI_ID): cv.use_id(SPIComponent),
                model.option(CONF_DC_PIN, cv.UNDEFINED): pins.gpio_output_pin_schema,
                model.option(CONF_DATA_RATE, "1MHz"): SPI_DATA_RATE_SCHEMA,
                model.option(CONF_SPI_MODE, "MODE0"): cv.enum(
                    SPI_MODE_OPTIONS, upper=True
                ),
                model.option(CONF_CS_PIN, cv.UNDEFINED): pins.gpio_output_pin_schema,
            }
        )
    return schema


def _config_schema(config):
    config = cv.Schema(
        {
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, upper=True),
        },
        extra=cv.ALLOW_EXTRA,
    )(config)
    schema = model_schema(config)
    return cv.All(
        schema,
        only_on_variant(supported=[VARIANT_ESP32S3]),
        cv.only_with_esp_idf,
    )(config)


CONFIG_SCHEMA = _config_schema


def _final_validate(config):
    global_config = full_config.get()

    from esphome.components.lvgl import DOMAIN as LVGL_DOMAIN

    if not requires_buffer(config) and LVGL_DOMAIN not in global_config:
        # If no drawing methods are configured, and LVGL is not enabled, show a test card
        config[CONF_SHOW_TEST_CARD] = True
    if CONF_SPI_ID in config:
        config = spi.final_validate_device_schema(
            "mipi_rgb", require_miso=False, require_mosi=True
        )(config)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    model = MODELS[config[CONF_MODEL].upper()]
    width, height, _offset_width, _offset_height = model.get_dimensions(config)
    var = cg.new_Pvariable(config[CONF_ID], width, height)
    cg.add(var.set_model(model.name))
    if enable_pin := config.get(CONF_ENABLE_PIN):
        enable = [await cg.gpio_pin_expression(pin) for pin in enable_pin]
        cg.add(var.set_enable_pins(enable))

    if CONF_SPI_ID in config:
        await spi.register_spi_device(var, config)
        sequence, madctl = model.get_sequence(config)
        cg.add(var.set_init_sequence(sequence))
        cg.add(var.set_madctl(madctl))

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
    dpins = []
    if CONF_RED in config[CONF_DATA_PINS]:
        red_pins = config[CONF_DATA_PINS][CONF_RED]
        green_pins = config[CONF_DATA_PINS][CONF_GREEN]
        blue_pins = config[CONF_DATA_PINS][CONF_BLUE]
        dpins.extend(blue_pins)
        dpins.extend(green_pins)
        dpins.extend(red_pins)
        # swap bytes to match big-endian format
        dpins = dpins[8:16] + dpins[0:8]
    else:
        dpins = config[CONF_DATA_PINS]
    for index, pin in enumerate(dpins):
        data_pin = await cg.gpio_pin_expression(pin)
        cg.add(var.add_data_pin(data_pin, index))

    if dc_pin := config.get(CONF_DC_PIN):
        dc = await cg.gpio_pin_expression(dc_pin)
        cg.add(var.set_dc_pin(dc))

    if reset_pin := config.get(CONF_RESET_PIN):
        reset = await cg.gpio_pin_expression(reset_pin)
        cg.add(var.set_reset_pin(reset))

    if model.rotation_as_transform(config):
        config[CONF_ROTATION] = 0

    if de_pin := config.get(CONF_DE_PIN):
        pin = await cg.gpio_pin_expression(de_pin)
        cg.add(var.set_de_pin(pin))
    pin = await cg.gpio_pin_expression(config[CONF_PCLK_PIN])
    cg.add(var.set_pclk_pin(pin))
    pin = await cg.gpio_pin_expression(config[CONF_HSYNC_PIN])
    cg.add(var.set_hsync_pin(pin))
    pin = await cg.gpio_pin_expression(config[CONF_VSYNC_PIN])
    cg.add(var.set_vsync_pin(pin))

    await display.register_display(var, config)
    if lamb := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lamb, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

import importlib
import logging
import pkgutil

from esphome import pins
import esphome.codegen as cg
from esphome.components import display, spi
from esphome.components.const import (
    CONF_BYTE_ORDER,
    CONF_COLOR_DEPTH,
    CONF_DRAW_ROUNDING,
)
from esphome.components.display import CONF_SHOW_TEST_CARD, DISPLAY_ROTATIONS
from esphome.components.mipi import (
    CONF_PIXEL_MODE,
    CONF_USE_AXIS_FLIPS,
    MADCTL,
    MODE_BGR,
    MODE_RGB,
    PIXFMT,
    DriverChip,
    dimension_schema,
    get_color_depth,
    map_sequence,
    power_of_two,
    requires_buffer,
)
from esphome.components.psram import DOMAIN as PSRAM_DOMAIN
from esphome.components.spi import TYPE_OCTAL, TYPE_QUAD, TYPE_SINGLE
import esphome.config_validation as cv
from esphome.config_validation import ALLOW_EXTRA
from esphome.const import (
    CONF_BRIGHTNESS,
    CONF_BUFFER_SIZE,
    CONF_COLOR_ORDER,
    CONF_CS_PIN,
    CONF_DATA_RATE,
    CONF_DC_PIN,
    CONF_DIMENSIONS,
    CONF_DISABLED,
    CONF_ENABLE_PIN,
    CONF_ID,
    CONF_INIT_SEQUENCE,
    CONF_INVERT_COLORS,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODEL,
    CONF_RESET_PIN,
    CONF_ROTATION,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
    CONF_WIDTH,
)
from esphome.core import CORE
from esphome.cpp_generator import TemplateArguments
from esphome.final_validate import full_config

from . import CONF_BUS_MODE, CONF_SPI_16, DOMAIN, models

DEPENDENCIES = ["spi"]

LOGGER = logging.getLogger(DOMAIN)
mipi_spi_ns = cg.esphome_ns.namespace("mipi_spi")
MipiSpi = mipi_spi_ns.class_("MipiSpi", display.Display, cg.Component, spi.SPIDevice)
MipiSpiBuffer = mipi_spi_ns.class_(
    "MipiSpiBuffer", MipiSpi, display.Display, cg.Component, spi.SPIDevice
)
ColorOrder = display.display_ns.enum("ColorMode")
ColorBitness = display.display_ns.enum("ColorBitness")
Model = mipi_spi_ns.enum("Model")

PixelMode = mipi_spi_ns.enum("PixelMode")
BusType = mipi_spi_ns.enum("BusType")

COLOR_ORDERS = {
    MODE_RGB: ColorOrder.COLOR_ORDER_RGB,
    MODE_BGR: ColorOrder.COLOR_ORDER_BGR,
}

COLOR_DEPTHS = {
    8: PixelMode.PIXEL_MODE_8,
    16: PixelMode.PIXEL_MODE_16,
    18: PixelMode.PIXEL_MODE_18,
}

DATA_PIN_SCHEMA = pins.internal_gpio_output_pin_schema

BusTypes = {
    TYPE_SINGLE: BusType.BUS_TYPE_SINGLE,
    TYPE_QUAD: BusType.BUS_TYPE_QUAD,
    TYPE_OCTAL: BusType.BUS_TYPE_OCTAL,
}

DriverChip("CUSTOM")

# Import all models dynamically from the models package
for module_info in pkgutil.iter_modules(models.__path__):
    importlib.import_module(f".models.{module_info.name}", package=__package__)

MODELS = DriverChip.get_models()


DISPLAY_18BIT = "18bit"
DISPLAY_16BIT = "16bit"

DISPLAY_PIXEL_MODES = {
    DISPLAY_16BIT: (0x55, PixelMode.PIXEL_MODE_16),
    DISPLAY_18BIT: (0x66, PixelMode.PIXEL_MODE_18),
}


def denominator(config):
    """
    Calculate the best denominator for a buffer size fraction.
    The denominator must be a number between 2 and 16 that divides the display height evenly,
    and the fraction represented by the denominator must be less than or equal to the given fraction.
    :config: The configuration dictionary containing the buffer size fraction and display dimensions
    :return: The denominator to use for the buffer size fraction
    """
    model = MODELS[config[CONF_MODEL]]
    frac = config.get(CONF_BUFFER_SIZE)
    if frac is None or frac > 0.75:
        return 1
    height, _width, _offset_width, _offset_height = model.get_dimensions(config)
    try:
        return next(x for x in range(2, 17) if frac >= 1 / x and height % x == 0)
    except StopIteration:
        raise cv.Invalid(
            f"Buffer size fraction {frac} is not compatible with display height {height}"
        ) from StopIteration


def model_schema(config):
    model = MODELS[config[CONF_MODEL]]
    bus_mode = config[CONF_BUS_MODE]
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
    # CUSTOM model will need to provide a custom init sequence
    iseqconf = (
        cv.Required(CONF_INIT_SEQUENCE)
        if model.initsequence is None
        else cv.Optional(CONF_INIT_SEQUENCE)
    )
    # Dimensions are optional if the model has a default width and the x-y transform is not overridden
    transform_config = config.get(CONF_TRANSFORM, {})
    is_swapped = (
        isinstance(transform_config, dict)
        and transform_config.get(CONF_SWAP_XY, False) is True
    )
    cv_dimensions = (
        cv.Optional if model.get_default(CONF_WIDTH) and not is_swapped else cv.Required
    )
    pixel_modes = DISPLAY_PIXEL_MODES if bus_mode == TYPE_SINGLE else (DISPLAY_16BIT,)
    color_depth = (
        ("16", "8", "16bit", "8bit") if bus_mode == TYPE_SINGLE else ("16", "16bit")
    )
    other_options = [
        CONF_INVERT_COLORS,
        CONF_USE_AXIS_FLIPS,
    ]
    if bus_mode == TYPE_SINGLE:
        other_options.append(CONF_SPI_16)
    schema = (
        display.FULL_DISPLAY_SCHEMA.extend(
            spi.spi_device_schema(
                cs_pin_required=False,
                default_mode="MODE3" if bus_mode == TYPE_OCTAL else "MODE0",
                default_data_rate=model.get_default(CONF_DATA_RATE, 10_000_000),
                mode=bus_mode,
            )
        )
        .extend(
            {
                model.option(pin, cv.UNDEFINED): pins.gpio_output_pin_schema
                for pin in (CONF_RESET_PIN, CONF_CS_PIN, CONF_DC_PIN)
            }
        )
        .extend(
            {
                cv.GenerateID(): cv.declare_id(MipiSpi),
                cv_dimensions(CONF_DIMENSIONS): dimension_schema(1),
                model.option(CONF_ENABLE_PIN, cv.UNDEFINED): cv.ensure_list(
                    pins.gpio_output_pin_schema
                ),
                model.option(CONF_COLOR_ORDER, MODE_BGR): cv.enum(
                    COLOR_ORDERS, upper=True
                ),
                model.option(CONF_BYTE_ORDER, "big_endian"): cv.one_of(
                    "big_endian", "little_endian", lower=True
                ),
                model.option(CONF_COLOR_DEPTH, 16): cv.one_of(*color_depth, lower=True),
                model.option(CONF_DRAW_ROUNDING, 2): power_of_two,
                model.option(CONF_PIXEL_MODE, DISPLAY_16BIT): cv.one_of(
                    *pixel_modes, lower=True
                ),
                cv.Optional(CONF_TRANSFORM): transform,
                cv.Optional(CONF_BUS_MODE, default=bus_mode): cv.one_of(
                    bus_mode, lower=True
                ),
                cv.Required(CONF_MODEL): cv.one_of(model.name, upper=True),
                iseqconf: cv.ensure_list(map_sequence),
                cv.Optional(CONF_BUFFER_SIZE): cv.All(
                    cv.percentage, cv.Range(0.12, 1.0)
                ),
            }
        )
        .extend({model.option(x): cv.boolean for x in other_options})
    )
    if brightness := model.get_default(CONF_BRIGHTNESS):
        schema = schema.extend(
            {
                cv.Optional(CONF_BRIGHTNESS, default=brightness): cv.int_range(
                    0, 0xFF, min_included=True, max_included=True
                ),
            }
        )
    if bus_mode != TYPE_SINGLE:
        return cv.All(schema, cv.only_with_esp_idf)
    return schema


def customise_schema(config):
    """
    Create a customised config schema for a specific model and validate the configuration.
    :param config: The configuration dictionary to validate
    :return: The validated configuration dictionary
    :raises cv.Invalid: If the configuration is invalid
    """
    # First get the model and bus mode
    config = cv.Schema(
        {
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, upper=True),
        },
        extra=ALLOW_EXTRA,
    )(config)
    model = MODELS[config[CONF_MODEL]]
    bus_modes = (TYPE_SINGLE, TYPE_QUAD, TYPE_OCTAL)
    config = cv.Schema(
        {
            model.option(CONF_BUS_MODE, TYPE_SINGLE): cv.one_of(*bus_modes, lower=True),
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, upper=True),
        },
        extra=ALLOW_EXTRA,
    )(config)
    bus_mode = config[CONF_BUS_MODE]
    config = model_schema(config)(config)
    # Check for invalid combinations of MADCTL config
    if init_sequence := config.get(CONF_INIT_SEQUENCE):
        commands = [x[0] for x in init_sequence]
        if MADCTL in commands and CONF_TRANSFORM in config:
            raise cv.Invalid(
                f"transform is not supported when MADCTL ({MADCTL:#X}) is in the init sequence"
            )
        if PIXFMT in commands:
            raise cv.Invalid(
                f"PIXFMT ({PIXFMT:#X}) should not be in the init sequence, it will be set automatically"
            )

    if bus_mode == TYPE_QUAD and CONF_DC_PIN in config:
        raise cv.Invalid("DC pin is not supported in quad mode")
    if bus_mode != TYPE_QUAD and CONF_DC_PIN not in config:
        raise cv.Invalid(f"DC pin is required in {bus_mode} mode")
    denominator(config)
    return config


CONFIG_SCHEMA = customise_schema


def _final_validate(config):
    global_config = full_config.get()
    model = MODELS[config[CONF_MODEL]]

    from esphome.components.lvgl import DOMAIN as LVGL_DOMAIN

    if not requires_buffer(config) and LVGL_DOMAIN not in global_config:
        # If no drawing methods are configured, and LVGL is not enabled, show a test card
        config[CONF_SHOW_TEST_CARD] = True

    if PSRAM_DOMAIN not in global_config and CONF_BUFFER_SIZE not in config:
        if not requires_buffer(config):
            return config  # No buffer needed, so no need to set a buffer size
        # If PSRAM is not enabled, choose a small buffer size by default
        if not requires_buffer(config):
            # not our problem.
            return config
        color_depth = get_color_depth(config)
        frac = denominator(config)
        height, width, _offset_width, _offset_height = model.get_dimensions(config)

        buffer_size = color_depth // 8 * width * height // frac
        # Target a buffer size of 20kB
        fraction = 20000.0 / buffer_size
        try:
            config[CONF_BUFFER_SIZE] = 1.0 / next(
                x for x in range(2, 17) if fraction >= 1 / x and height % x == 0
            )
        except StopIteration:
            # Either the screen is too big, or the height is not divisible by any of the fractions, so use 1.0
            # PSRAM will be needed.
            if CORE.is_esp32:
                raise cv.Invalid(
                    "PSRAM is required for this display"
                ) from StopIteration

    return config


FINAL_VALIDATE_SCHEMA = _final_validate


def get_transform(config):
    """
    Get the transformation configuration for the display.
    :param config:
    :return:
    """
    model = MODELS[config[CONF_MODEL]]
    can_transform = model.rotation_as_transform(config)
    transform = config.get(
        CONF_TRANSFORM,
        {
            CONF_MIRROR_X: model.get_default(CONF_MIRROR_X, False),
            CONF_MIRROR_Y: model.get_default(CONF_MIRROR_Y, False),
            CONF_SWAP_XY: model.get_default(CONF_SWAP_XY, False),
        },
    )

    # Can we use the MADCTL register to set the rotation?
    if can_transform and CONF_TRANSFORM not in config:
        rotation = config[CONF_ROTATION]
        if rotation == 180:
            transform[CONF_MIRROR_X] = not transform[CONF_MIRROR_X]
            transform[CONF_MIRROR_Y] = not transform[CONF_MIRROR_Y]
        elif rotation == 90:
            transform[CONF_SWAP_XY] = not transform[CONF_SWAP_XY]
            transform[CONF_MIRROR_X] = not transform[CONF_MIRROR_X]
        else:
            transform[CONF_SWAP_XY] = not transform[CONF_SWAP_XY]
            transform[CONF_MIRROR_Y] = not transform[CONF_MIRROR_Y]
        transform[CONF_TRANSFORM] = True
    return transform


def get_instance(config):
    """
    Get the type of MipiSpi instance to create based on the configuration,
    and the template arguments.
    :param config:
    :return: type, template arguments
    """
    model = MODELS[config[CONF_MODEL]]
    width, height, offset_width, offset_height = model.get_dimensions(config)

    color_depth = int(config[CONF_COLOR_DEPTH].removesuffix("bit"))
    bufferpixels = COLOR_DEPTHS[color_depth]

    display_pixel_mode = DISPLAY_PIXEL_MODES[config[CONF_PIXEL_MODE]][1]
    bus_type = config[CONF_BUS_MODE]
    if bus_type == TYPE_SINGLE and config.get(CONF_SPI_16, False):
        # If the bus mode is single and spi_16 is set, use single 16-bit mode
        bus_type = BusType.BUS_TYPE_SINGLE_16
    else:
        bus_type = BusTypes[bus_type]
    buffer_type = cg.uint8 if color_depth == 8 else cg.uint16
    frac = denominator(config)
    rotation = (
        0 if model.rotation_as_transform(config) else config.get(CONF_ROTATION, 0)
    )
    templateargs = [
        buffer_type,
        bufferpixels,
        config[CONF_BYTE_ORDER] == "big_endian",
        display_pixel_mode,
        bus_type,
    ]
    # If a buffer is required, use MipiSpiBuffer, otherwise use MipiSpi
    if requires_buffer(config):
        templateargs.extend(
            [
                width,
                height,
                offset_width,
                offset_height,
                DISPLAY_ROTATIONS[rotation],
                frac,
                config[CONF_DRAW_ROUNDING],
            ]
        )
        return MipiSpiBuffer, templateargs
    # Swap height and width if the display is rotated 90 or 270 degrees in software
    if rotation in (90, 270):
        width, height = height, width
        offset_width, offset_height = offset_height, offset_width
    templateargs.extend(
        [
            width,
            height,
            offset_width,
            offset_height,
        ]
    )
    return MipiSpi, templateargs


async def to_code(config):
    model = MODELS[config[CONF_MODEL]]
    var_id = config[CONF_ID]
    var_id.type, templateargs = get_instance(config)
    var = cg.new_Pvariable(var_id, TemplateArguments(*templateargs))
    init_sequence, _madctl = model.get_sequence(config)
    cg.add(var.set_init_sequence(init_sequence))
    if model.rotation_as_transform(config):
        if CONF_TRANSFORM in config:
            LOGGER.warning("Use of 'transform' with 'rotation' is not recommended")
        else:
            config[CONF_ROTATION] = 0
    cg.add(var.set_model(config[CONF_MODEL]))
    if enable_pin := config.get(CONF_ENABLE_PIN):
        enable = [await cg.gpio_pin_expression(pin) for pin in enable_pin]
        cg.add(var.set_enable_pins(enable))

    if reset_pin := config.get(CONF_RESET_PIN):
        reset = await cg.gpio_pin_expression(reset_pin)
        cg.add(var.set_reset_pin(reset))

    if dc_pin := config.get(CONF_DC_PIN):
        dc_pin = await cg.gpio_pin_expression(dc_pin)
        cg.add(var.set_dc_pin(dc_pin))

    if lamb := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lamb, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)
    # Displays are write-only, set the SPI device to write-only as well
    cg.add(var.set_write_only(True))

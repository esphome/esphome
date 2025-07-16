import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import display, spi
from esphome.components.const import (
    CONF_BYTE_ORDER,
    CONF_COLOR_DEPTH,
    CONF_DRAW_ROUNDING,
)
from esphome.components.display import CONF_SHOW_TEST_CARD, DISPLAY_ROTATIONS
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
    CONF_ENABLE_PIN,
    CONF_HEIGHT,
    CONF_ID,
    CONF_INIT_SEQUENCE,
    CONF_INVERT_COLORS,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODEL,
    CONF_OFFSET_HEIGHT,
    CONF_OFFSET_WIDTH,
    CONF_PAGES,
    CONF_RESET_PIN,
    CONF_ROTATION,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
    CONF_WIDTH,
)
from esphome.core import CORE, TimePeriod
from esphome.cpp_generator import TemplateArguments
from esphome.final_validate import full_config

from . import (
    CONF_BUS_MODE,
    CONF_NATIVE_HEIGHT,
    CONF_NATIVE_WIDTH,
    CONF_PIXEL_MODE,
    CONF_SPI_16,
    CONF_USE_AXIS_FLIPS,
    DOMAIN,
    MODE_BGR,
    MODE_RGB,
)
from .models import (
    DELAY_FLAG,
    MADCTL_BGR,
    MADCTL_MV,
    MADCTL_MX,
    MADCTL_MY,
    MADCTL_XFLIP,
    MADCTL_YFLIP,
    DriverChip,
    adafruit,
    amoled,
    cyd,
    ili,
    jc,
    lanbon,
    lilygo,
    waveshare,
)
from .models.commands import BRIGHTNESS, DISPON, INVOFF, INVON, MADCTL, PIXFMT, SLPOUT

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

MODELS = DriverChip.models
# This loop is a noop, but suppresses linting of side-effect-only imports
for _ in (ili, jc, amoled, lilygo, lanbon, cyd, waveshare, adafruit):
    pass


DISPLAY_18BIT = "18bit"
DISPLAY_16BIT = "16bit"

DISPLAY_PIXEL_MODES = {
    DISPLAY_16BIT: (0x55, PixelMode.PIXEL_MODE_16),
    DISPLAY_18BIT: (0x66, PixelMode.PIXEL_MODE_18),
}


def get_dimensions(config):
    if CONF_DIMENSIONS in config:
        # Explicit dimensions, just use as is
        dimensions = config[CONF_DIMENSIONS]
        if isinstance(dimensions, dict):
            width = dimensions[CONF_WIDTH]
            height = dimensions[CONF_HEIGHT]
            offset_width = dimensions[CONF_OFFSET_WIDTH]
            offset_height = dimensions[CONF_OFFSET_HEIGHT]
            return width, height, offset_width, offset_height
        (width, height) = dimensions
        return width, height, 0, 0

    # Default dimensions, use model defaults
    transform = get_transform(config)

    model = MODELS[config[CONF_MODEL]]
    width = model.get_default(CONF_WIDTH)
    height = model.get_default(CONF_HEIGHT)
    offset_width = model.get_default(CONF_OFFSET_WIDTH, 0)
    offset_height = model.get_default(CONF_OFFSET_HEIGHT, 0)

    # if mirroring axes and there are offsets, also mirror the offsets to cater for situations where
    # the offset is asymmetric
    if transform[CONF_MIRROR_X]:
        native_width = model.get_default(CONF_NATIVE_WIDTH, width + offset_width * 2)
        offset_width = native_width - width - offset_width
    if transform[CONF_MIRROR_Y]:
        native_height = model.get_default(
            CONF_NATIVE_HEIGHT, height + offset_height * 2
        )
        offset_height = native_height - height - offset_height
    # Swap default dimensions if swap_xy is set
    if transform[CONF_SWAP_XY] is True:
        width, height = height, width
        offset_height, offset_width = offset_width, offset_height
    return width, height, offset_width, offset_height


def denominator(config):
    """
    Calculate the best denominator for a buffer size fraction.
    The denominator must be a number between 2 and 16 that divides the display height evenly,
    and the fraction represented by the denominator must be less than or equal to the given fraction.
    :config: The configuration dictionary containing the buffer size fraction and display dimensions
    :return: The denominator to use for the buffer size fraction
    """
    frac = config.get(CONF_BUFFER_SIZE)
    if frac is None or frac > 0.75:
        return 1
    height, _width, _offset_width, _offset_height = get_dimensions(config)
    try:
        return next(x for x in range(2, 17) if frac >= 1 / x and height % x == 0)
    except StopIteration:
        raise cv.Invalid(
            f"Buffer size fraction {frac} is not compatible with display height {height}"
        ) from StopIteration


def validate_dimension(rounding):
    def validator(value):
        value = cv.positive_int(value)
        if value % rounding != 0:
            raise cv.Invalid(f"Dimensions and offsets must be divisible by {rounding}")
        return value

    return validator


def map_sequence(value):
    """
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
        return DELAY_FLAG, delay.total_milliseconds
    if isinstance(value, int):
        return (value,)
    value = cv.All(cv.ensure_list(cv.int_range(0, 255)), cv.Length(1, 254))(value)
    return tuple(value)


def power_of_two(value):
    value = cv.int_range(1, 128)(value)
    if value & (value - 1) != 0:
        raise cv.Invalid("value must be a power of two")
    return value


def dimension_schema(rounding):
    return cv.Any(
        cv.dimensions,
        cv.Schema(
            {
                cv.Required(CONF_WIDTH): validate_dimension(rounding),
                cv.Required(CONF_HEIGHT): validate_dimension(rounding),
                cv.Optional(CONF_OFFSET_HEIGHT, default=0): validate_dimension(
                    rounding
                ),
                cv.Optional(CONF_OFFSET_WIDTH, default=0): validate_dimension(rounding),
            }
        ),
    )


def swap_xy_schema(model):
    uses_swap = model.get_default(CONF_SWAP_XY, None) != cv.UNDEFINED

    def validator(value):
        if value:
            raise cv.Invalid("Axis swapping not supported by this model")
        return cv.boolean(value)

    if uses_swap:
        return {cv.Required(CONF_SWAP_XY): cv.boolean}
    return {cv.Optional(CONF_SWAP_XY, default=False): validator}


def model_schema(config):
    model = MODELS[config[CONF_MODEL]]
    bus_mode = config.get(CONF_BUS_MODE, model.modes[0])
    transform = cv.Schema(
        {
            cv.Required(CONF_MIRROR_X): cv.boolean,
            cv.Required(CONF_MIRROR_Y): cv.boolean,
            **swap_xy_schema(model),
        }
    )
    # CUSTOM model will need to provide a custom init sequence
    iseqconf = (
        cv.Required(CONF_INIT_SEQUENCE)
        if model.initsequence is None
        else cv.Optional(CONF_INIT_SEQUENCE)
    )
    # Dimensions are optional if the model has a default width and the x-y transform is not overridden
    is_swapped = config.get(CONF_TRANSFORM, {}).get(CONF_SWAP_XY) is True
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
                cv_dimensions(CONF_DIMENSIONS): dimension_schema(
                    model.get_default(CONF_DRAW_ROUNDING, 1)
                ),
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


def is_rotation_transformable(config):
    """
    Check if a rotation can be implemented in hardware using the MADCTL register.
    A rotation of 180 is always possible, 90 and 270 are possible if the model supports swapping X and Y.
    """
    model = MODELS[config[CONF_MODEL]]
    rotation = config.get(CONF_ROTATION, 0)
    return rotation and (
        model.get_default(CONF_SWAP_XY) != cv.UNDEFINED or rotation == 180
    )


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
    bus_modes = model.modes
    config = cv.Schema(
        {
            model.option(CONF_BUS_MODE, TYPE_SINGLE): cv.one_of(*bus_modes, lower=True),
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, upper=True),
        },
        extra=ALLOW_EXTRA,
    )(config)
    bus_mode = config.get(CONF_BUS_MODE, model.modes[0])
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


def requires_buffer(config):
    """
    Check if the display configuration requires a buffer. It will do so if any drawing methods are configured.
    :param config:
    :return:  True if a buffer is required, False otherwise
    """
    return any(
        config.get(key) for key in (CONF_LAMBDA, CONF_PAGES, CONF_SHOW_TEST_CARD)
    )


def get_color_depth(config):
    return int(config[CONF_COLOR_DEPTH].removesuffix("bit"))


def _final_validate(config):
    global_config = full_config.get()

    from esphome.components.lvgl import DOMAIN as LVGL_DOMAIN

    if not requires_buffer(config) and LVGL_DOMAIN not in global_config:
        # If no drawing methods are configured, and LVGL is not enabled, show a test card
        config[CONF_SHOW_TEST_CARD] = True

    if "psram" not in global_config and CONF_BUFFER_SIZE not in config:
        if not requires_buffer(config):
            return config  # No buffer needed, so no need to set a buffer size
        # If PSRAM is not enabled, choose a small buffer size by default
        if not requires_buffer(config):
            # not our problem.
            return config
        color_depth = get_color_depth(config)
        frac = denominator(config)
        height, width, _offset_width, _offset_height = get_dimensions(config)

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
    can_transform = is_rotation_transformable(config)
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


def get_sequence(model, config):
    """
    Create the init sequence for the display.
    Use the default sequence from the model, if any, and append any custom sequence provided in the config.
    Append SLPOUT (if not already in the sequence) and DISPON to the end of the sequence
    Pixel format, color order, and orientation will be set.
    """
    sequence = list(model.initsequence)
    custom_sequence = config.get(CONF_INIT_SEQUENCE, [])
    sequence.extend(custom_sequence)
    # Ensure each command is a tuple
    sequence = [x if isinstance(x, tuple) else (x,) for x in sequence]
    commands = [x[0] for x in sequence]
    # Set pixel format if not already in the custom sequence
    pixel_mode = DISPLAY_PIXEL_MODES[config[CONF_PIXEL_MODE]]
    sequence.append((PIXFMT, pixel_mode[0]))
    # Does the chip use the flipping bits for mirroring rather than the reverse order bits?
    use_flip = config[CONF_USE_AXIS_FLIPS]
    if MADCTL not in commands:
        madctl = 0
        transform = get_transform(config)
        if transform.get(CONF_TRANSFORM):
            LOGGER.info("Using hardware transform to implement rotation")
        if transform.get(CONF_MIRROR_X):
            madctl |= MADCTL_XFLIP if use_flip else MADCTL_MX
        if transform.get(CONF_MIRROR_Y):
            madctl |= MADCTL_YFLIP if use_flip else MADCTL_MY
        if transform.get(CONF_SWAP_XY) is True:  # Exclude Undefined
            madctl |= MADCTL_MV
        if config[CONF_COLOR_ORDER] == MODE_BGR:
            madctl |= MADCTL_BGR
        sequence.append((MADCTL, madctl))
    if INVON not in commands and INVOFF not in commands:
        if config[CONF_INVERT_COLORS]:
            sequence.append((INVON,))
        else:
            sequence.append((INVOFF,))
    if BRIGHTNESS not in commands:
        if brightness := config.get(
            CONF_BRIGHTNESS, model.get_default(CONF_BRIGHTNESS)
        ):
            sequence.append((BRIGHTNESS, brightness))
    if SLPOUT not in commands:
        sequence.append((SLPOUT,))
    sequence.append((DISPON,))

    # Flatten the sequence into a list of bytes, with the length of each command
    # or the delay flag inserted where needed
    return sum(
        tuple(
            (x[1], 0xFF) if x[0] == DELAY_FLAG else (x[0], len(x) - 1) + x[1:]
            for x in sequence
        ),
        (),
    )


def get_instance(config):
    """
    Get the type of MipiSpi instance to create based on the configuration,
    and the template arguments.
    :param config:
    :return: type, template arguments
    """
    width, height, offset_width, offset_height = get_dimensions(config)

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
    rotation = DISPLAY_ROTATIONS[
        0 if is_rotation_transformable(config) else config.get(CONF_ROTATION, 0)
    ]
    templateargs = [
        buffer_type,
        bufferpixels,
        config[CONF_BYTE_ORDER] == "big_endian",
        display_pixel_mode,
        bus_type,
        width,
        height,
        offset_width,
        offset_height,
    ]
    # If a buffer is required, use MipiSpiBuffer, otherwise use MipiSpi
    if requires_buffer(config):
        templateargs.append(rotation)
        templateargs.append(frac)
        return MipiSpiBuffer, templateargs
    return MipiSpi, templateargs


async def to_code(config):
    model = MODELS[config[CONF_MODEL]]
    var_id = config[CONF_ID]
    var_id.type, templateargs = get_instance(config)
    var = cg.new_Pvariable(var_id, TemplateArguments(*templateargs))
    cg.add(var.set_init_sequence(get_sequence(model, config)))
    if is_rotation_transformable(config):
        if CONF_TRANSFORM in config:
            LOGGER.warning("Use of 'transform' with 'rotation' is not recommended")
        else:
            config[CONF_ROTATION] = 0
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_draw_rounding(config[CONF_DRAW_ROUNDING]))
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

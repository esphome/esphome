import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import display, spi
from esphome.components.spi import TYPE_OCTAL, TYPE_QUAD, TYPE_SINGLE
import esphome.config_validation as cv
from esphome.config_validation import ALLOW_EXTRA
from esphome.const import (
    CONF_BRIGHTNESS,
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
    CONF_RESET_PIN,
    CONF_ROTATION,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
    CONF_WIDTH,
)
from esphome.core import TimePeriod

from ..const import CONF_DRAW_ROUNDING
from ..lvgl.defines import CONF_COLOR_DEPTH
from . import (
    CONF_BUS_MODE,
    CONF_DRAW_FROM_ORIGIN,
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
MipiSpi = mipi_spi_ns.class_(
    "MipiSpi", display.Display, display.DisplayBuffer, cg.Component, spi.SPIDevice
)
ColorOrder = display.display_ns.enum("ColorMode")
ColorBitness = display.display_ns.enum("ColorBitness")
Model = mipi_spi_ns.enum("Model")

COLOR_ORDERS = {
    MODE_RGB: ColorOrder.COLOR_ORDER_RGB,
    MODE_BGR: ColorOrder.COLOR_ORDER_BGR,
}

COLOR_DEPTHS = {
    8: ColorBitness.COLOR_BITNESS_332,
    16: ColorBitness.COLOR_BITNESS_565,
}
DATA_PIN_SCHEMA = pins.internal_gpio_output_pin_schema


DriverChip("CUSTOM", initsequence={})

MODELS = DriverChip.models
# These statements are noops, but serve to suppress linting of side-effect-only imports
for _ in (ili, jc, amoled, lilygo, lanbon, cyd, waveshare):
    pass

PixelMode = mipi_spi_ns.enum("PixelMode")

PIXEL_MODE_18BIT = "18bit"
PIXEL_MODE_16BIT = "16bit"

PIXEL_MODES = {
    PIXEL_MODE_16BIT: 0x55,
    PIXEL_MODE_18BIT: 0x66,
}


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


def model_schema(bus_mode, model: DriverChip, swapsies: bool):
    transform = cv.Schema(
        {
            cv.Required(CONF_MIRROR_X): cv.boolean,
            cv.Required(CONF_MIRROR_Y): cv.boolean,
        }
    )
    if model.get_default(CONF_SWAP_XY, False) == cv.UNDEFINED:
        transform = transform.extend(
            {
                cv.Optional(CONF_SWAP_XY): cv.invalid(
                    "Axis swapping not supported by this model"
                )
            }
        )
    else:
        transform = transform.extend(
            {
                cv.Required(CONF_SWAP_XY): cv.boolean,
            }
        )
    # CUSTOM model will need to provide a custom init sequence
    iseqconf = (
        cv.Required(CONF_INIT_SEQUENCE)
        if model.initsequence is None
        else cv.Optional(CONF_INIT_SEQUENCE)
    )
    # Dimensions are optional if the model has a default width and the transform is not overridden
    cv_dimensions = (
        cv.Optional if model.get_default(CONF_WIDTH) and not swapsies else cv.Required
    )
    pixel_modes = PIXEL_MODES if bus_mode == TYPE_SINGLE else (PIXEL_MODE_16BIT,)
    color_depth = (
        ("16", "8", "16bit", "8bit") if bus_mode == TYPE_SINGLE else ("16", "16bit")
    )
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
                model.option(CONF_COLOR_DEPTH, 16): cv.one_of(*color_depth, lower=True),
                model.option(CONF_DRAW_ROUNDING, 2): power_of_two,
                model.option(CONF_PIXEL_MODE, PIXEL_MODE_16BIT): cv.Any(
                    cv.one_of(*pixel_modes, lower=True),
                    cv.int_range(0, 255, min_included=True, max_included=True),
                ),
                cv.Optional(CONF_TRANSFORM): transform,
                cv.Optional(CONF_BUS_MODE, default=bus_mode): cv.one_of(
                    bus_mode, lower=True
                ),
                cv.Required(CONF_MODEL): cv.one_of(model.name, upper=True),
                iseqconf: cv.ensure_list(map_sequence),
            }
        )
        .extend(
            {
                model.option(x): cv.boolean
                for x in [
                    CONF_DRAW_FROM_ORIGIN,
                    CONF_SPI_16,
                    CONF_INVERT_COLORS,
                    CONF_USE_AXIS_FLIPS,
                ]
            }
        )
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


def rotation_as_transform(model, config):
    """
    Check if a rotation can be implemented in hardware using the MADCTL register.
    A rotation of 180 is always possible, 90 and 270 are possible if the model supports swapping X and Y.
    """
    rotation = config.get(CONF_ROTATION, 0)
    return rotation and (
        model.get_default(CONF_SWAP_XY) != cv.UNDEFINED or rotation == 180
    )


def config_schema(config):
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
    swapsies = config.get(CONF_TRANSFORM, {}).get(CONF_SWAP_XY) is True
    config = model_schema(bus_mode, model, swapsies)(config)
    # Check for invalid combinations of MADCTL config
    if init_sequence := config.get(CONF_INIT_SEQUENCE):
        if MADCTL in [x[0] for x in init_sequence] and CONF_TRANSFORM in config:
            raise cv.Invalid(
                f"transform is not supported when MADCTL ({MADCTL:#X}) is in the init sequence"
            )

    if bus_mode == TYPE_QUAD and CONF_DC_PIN in config:
        raise cv.Invalid("DC pin is not supported in quad mode")
    if config[CONF_PIXEL_MODE] == PIXEL_MODE_18BIT and bus_mode != TYPE_SINGLE:
        raise cv.Invalid("18-bit pixel mode is not supported on a quad or octal bus")
    if bus_mode != TYPE_QUAD and CONF_DC_PIN not in config:
        raise cv.Invalid(f"DC pin is required in {bus_mode} mode")
    return config


CONFIG_SCHEMA = config_schema


def get_transform(model, config):
    can_transform = rotation_as_transform(model, config)
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
    if PIXFMT not in commands:
        pixel_mode = config[CONF_PIXEL_MODE]
        if not isinstance(pixel_mode, int):
            pixel_mode = PIXEL_MODES[pixel_mode]
        sequence.append((PIXFMT, pixel_mode))
    # Does the chip use the flipping bits for mirroring rather than the reverse order bits?
    use_flip = config[CONF_USE_AXIS_FLIPS]
    if MADCTL not in commands:
        madctl = 0
        transform = get_transform(model, config)
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


async def to_code(config):
    model = MODELS[config[CONF_MODEL]]
    transform = get_transform(model, config)
    if CONF_DIMENSIONS in config:
        # Explicit dimensions, just use as is
        dimensions = config[CONF_DIMENSIONS]
        if isinstance(dimensions, dict):
            width = dimensions[CONF_WIDTH]
            height = dimensions[CONF_HEIGHT]
            offset_width = dimensions[CONF_OFFSET_WIDTH]
            offset_height = dimensions[CONF_OFFSET_HEIGHT]
        else:
            (width, height) = dimensions
            offset_width = 0
            offset_height = 0
    else:
        # Default dimensions, use model defaults and transform if needed
        width = model.get_default(CONF_WIDTH)
        height = model.get_default(CONF_HEIGHT)
        offset_width = model.get_default(CONF_OFFSET_WIDTH, 0)
        offset_height = model.get_default(CONF_OFFSET_HEIGHT, 0)

        # if mirroring axes and there are offsets, also mirror the offsets to cater for situations where
        # the offset is asymmetric
        if transform[CONF_MIRROR_X]:
            native_width = model.get_default(
                CONF_NATIVE_WIDTH, width + offset_width * 2
            )
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

    color_depth = config[CONF_COLOR_DEPTH]
    if color_depth.endswith("bit"):
        color_depth = color_depth[:-3]
    color_depth = COLOR_DEPTHS[int(color_depth)]

    var = cg.new_Pvariable(
        config[CONF_ID], width, height, offset_width, offset_height, color_depth
    )
    cg.add(var.set_init_sequence(get_sequence(model, config)))
    if rotation_as_transform(model, config):
        if CONF_TRANSFORM in config:
            LOGGER.warning("Use of 'transform' with 'rotation' is not recommended")
        else:
            config[CONF_ROTATION] = 0
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_draw_from_origin(config[CONF_DRAW_FROM_ORIGIN]))
    cg.add(var.set_draw_rounding(config[CONF_DRAW_ROUNDING]))
    cg.add(var.set_spi_16(config[CONF_SPI_16]))
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

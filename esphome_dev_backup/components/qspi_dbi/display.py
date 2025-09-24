from esphome import pins
import esphome.codegen as cg
from esphome.components import display, spi
from esphome.components.const import CONF_DRAW_ROUNDING
import esphome.config_validation as cv
from esphome.const import (
    CONF_BRIGHTNESS,
    CONF_COLOR_ORDER,
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
    CONF_SWAP_XY,
    CONF_TRANSFORM,
    CONF_WIDTH,
)
from esphome.core import TimePeriod

from . import CONF_DRAW_FROM_ORIGIN
from .models import DriverChip

DEPENDENCIES = ["spi"]

qspi_dbi_ns = cg.esphome_ns.namespace("qspi_dbi")
QSPI_DBI = qspi_dbi_ns.class_(
    "QspiDbi", display.Display, display.DisplayBuffer, cg.Component, spi.SPIDevice
)
ColorOrder = display.display_ns.enum("ColorMode")
Model = qspi_dbi_ns.enum("Model")

COLOR_ORDERS = {
    "RGB": ColorOrder.COLOR_ORDER_RGB,
    "BGR": ColorOrder.COLOR_ORDER_BGR,
}
DATA_PIN_SCHEMA = pins.internal_gpio_output_pin_schema

DELAY_FLAG = 0xFF


def validate_dimension(value):
    value = cv.positive_int(value)
    if value % 2 != 0:
        raise cv.Invalid("Width/height/offset must be divisible by 2")
    return value


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
        return [delay, DELAY_FLAG]
    value = cv.Length(min=1, max=254)(value)
    params = value[1:]
    return [value[0], len(params)] + list(params)


def _validate(config):
    chip = DriverChip.chips[config[CONF_MODEL]]
    if not chip.initsequence:
        if CONF_INIT_SEQUENCE not in config:
            raise cv.Invalid(f"{chip.name} model requires init_sequence")
    return config


def power_of_two(value):
    value = cv.int_range(1, 128)(value)
    if value & (value - 1) != 0:
        raise cv.Invalid("value must be a power of two")
    return value


BASE_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(QSPI_DBI),
            cv.Optional(CONF_INIT_SEQUENCE): cv.ensure_list(map_sequence),
            cv.Required(CONF_DIMENSIONS): cv.Any(
                cv.dimensions,
                cv.Schema(
                    {
                        cv.Required(CONF_WIDTH): validate_dimension,
                        cv.Required(CONF_HEIGHT): validate_dimension,
                        cv.Optional(CONF_OFFSET_HEIGHT, default=0): validate_dimension,
                        cv.Optional(CONF_OFFSET_WIDTH, default=0): validate_dimension,
                    }
                ),
            ),
            cv.Optional(CONF_DRAW_FROM_ORIGIN, default=False): cv.boolean,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BRIGHTNESS, default=0xD0): cv.int_range(
                0, 0xFF, min_included=True, max_included=True
            ),
        }
    ).extend(
        spi.spi_device_schema(
            cs_pin_required=False,
            default_mode="MODE0",
            default_data_rate=10e6,
            mode=spi.TYPE_QUAD,
        )
    )
)


def model_property(name, defaults, fallback):
    return cv.Optional(name, default=defaults.get(name, fallback))


def model_schema(defaults):
    transform = cv.Schema(
        {
            cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
            cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
        }
    )
    if defaults.get(CONF_SWAP_XY, True):
        transform = transform.extend(
            {
                cv.Optional(CONF_SWAP_XY, default=False): cv.boolean,
            }
        )
    return BASE_SCHEMA.extend(
        {
            model_property(CONF_INVERT_COLORS, defaults, False): cv.boolean,
            model_property(CONF_COLOR_ORDER, defaults, "RGB"): cv.enum(
                COLOR_ORDERS, upper=True
            ),
            model_property(CONF_DRAW_ROUNDING, defaults, 2): power_of_two,
            cv.Optional(CONF_TRANSFORM): transform,
        }
    )


CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {k.upper(): model_schema(v.defaults) for k, v in DriverChip.chips.items()},
        upper=True,
        key=CONF_MODEL,
    ),
    cv.only_with_esp_idf,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    chip = DriverChip.chips[config[CONF_MODEL]]
    if chip.initsequence:
        cg.add(var.add_init_sequence(chip.initsequence))
    if init_sequences := config.get(CONF_INIT_SEQUENCE):
        sequence = []
        for seq in init_sequences:
            sequence.extend(seq)
        cg.add(var.add_init_sequence(sequence))

    cg.add(var.set_color_mode(config[CONF_COLOR_ORDER]))
    cg.add(var.set_invert_colors(config[CONF_INVERT_COLORS]))
    cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_draw_from_origin(config[CONF_DRAW_FROM_ORIGIN]))
    cg.add(var.set_draw_rounding(config[CONF_DRAW_ROUNDING]))
    if enable_pin := config.get(CONF_ENABLE_PIN):
        enable = await cg.gpio_pin_expression(enable_pin)
        cg.add(var.set_enable_pin(enable))

    if reset_pin := config.get(CONF_RESET_PIN):
        reset = await cg.gpio_pin_expression(reset_pin)
        cg.add(var.set_reset_pin(reset))

    if transform := config.get(CONF_TRANSFORM):
        cg.add(var.set_mirror_x(transform[CONF_MIRROR_X]))
        cg.add(var.set_mirror_y(transform[CONF_MIRROR_Y]))
        # swap_xy is not implemented for some chips
        cg.add(var.set_swap_xy(transform.get(CONF_SWAP_XY, False)))

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

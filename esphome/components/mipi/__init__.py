# Various constants used in MIPI DBI communication
# Various configuration constants for MIPI displays
# Various utility functions for MIPI DBI configuration

from collections.abc import Callable
import functools
from typing import Any, Self

import voluptuous as vol

from esphome.components.const import CONF_COLOR_DEPTH
from esphome.components.display import CONF_SHOW_TEST_CARD, display_ns
import esphome.config_validation as cv
from esphome.const import (
    CONF_BRIGHTNESS,
    CONF_COLOR_ORDER,
    CONF_DIMENSIONS,
    CONF_DISABLED,
    CONF_HEIGHT,
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
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor

LOGGER = cv.logging.getLogger(__name__)

CONF_TRANSFORMS = "transforms"

# All axis transforms a model may support, in the order they appear in the schema.
ALL_TRANSFORMS = (CONF_MIRROR_X, CONF_MIRROR_Y, CONF_SWAP_XY)

ColorOrder = display_ns.enum("ColorMode")

NOP = 0x00
SWRESET = 0x01
RDDID = 0x04
RDDST = 0x09
RDMODE = 0x0A
RDMADCTL = 0x0B
RDPIXFMT = 0x0C
RDIMGFMT = 0x0D
RDSELFDIAG = 0x0F
SLEEP_IN = 0x10
SLPIN = 0x10
SLEEP_OUT = 0x11
SLPOUT = 0x11
PTLON = 0x12
NORON = 0x13
INVERT_OFF = 0x20
INVOFF = 0x20
INVERT_ON = 0x21
INVON = 0x21
ALL_ON = 0x23
WRAM = 0x24
GAMMASET = 0x26
MIPI = 0x26
DISPOFF = 0x28
DISPON = 0x29
CASET = 0x2A
PASET = 0x2B
RASET = 0x2B
RAMWR = 0x2C
WDATA = 0x2C
RAMRD = 0x2E
PTLAR = 0x30
VSCRDEF = 0x33
TEON = 0x35
MADCTL = 0x36
MADCTL_CMD = 0x36
VSCRSADD = 0x37
IDMOFF = 0x38
IDMON = 0x39
COLMOD = 0x3A
PIXFMT = 0x3A
GETSCANLINE = 0x45
BRIGHTNESS = 0x51
WRDISBV = 0x51
RDDISBV = 0x52
WRCTRLD = 0x53
WCE = 0x58
SWIRE1 = 0x5A
SWIRE2 = 0x5B
IFMODE = 0xB0
FRMCTR1 = 0xB1
FRMCTR2 = 0xB2
FRMCTR3 = 0xB3
INVCTR = 0xB4
DFUNCTR = 0xB6
ETMOD = 0xB7
PWCTR1 = 0xC0
PWCTR2 = 0xC1
PWCTR3 = 0xC2
PWCTR4 = 0xC3
PWCTR5 = 0xC4
SPIMODESEL = 0xC4
VMCTR1 = 0xC5
IFCTR = 0xC6
VMCTR2 = 0xC7
GMCTR = 0xC8
SETEXTC = 0xC8
PWSET = 0xD0
VMCTR = 0xD1
PWSETN = 0xD2
RDID4 = 0xD3
RDINDEX = 0xD9
RDID1 = 0xDA
RDID2 = 0xDB
RDID3 = 0xDC
RDIDX = 0xDD
GMCTRP1 = 0xE0
GMCTRN1 = 0xE1
CSCON = 0xF0
PWCTR6 = 0xF6
ADJCTL3 = 0xF7
PAGESEL = 0xFE
PAGESEL1 = 0xFF

MADCTL_MY = 0x80  # Bit 7 Bottom to top
MADCTL_MX = 0x40  # Bit 6 Right to left
MADCTL_MV = 0x20  # Bit 5 Reverse Mode
MADCTL_ML = 0x10  # Bit 4 LCD refresh Bottom to top
MADCTL_RGB = 0x00  # Bit 3 Red-Green-Blue pixel order
MADCTL_BGR = 0x08  # Bit 3 Blue-Green-Red pixel order
MADCTL_MH = 0x04  # Bit 2 LCD refresh right to left

# These bits are used instead of the above bits on some chips, where using MX and MY results in incorrect
# partial updates.
MADCTL_XFLIP = 0x02  # Mirror the display horizontally
MADCTL_YFLIP = 0x01  # Mirror the display vertically

MADCTL_FLIP_FLAG = 0x100  # meta-flag to indicate use of axis flips

# Special constant for delays in command sequences
DELAY_FLAG = 0xFFF  # Special flag to indicate a delay

CONF_PAD_HEIGHT = "pad_height"
CONF_PAD_WIDTH = "pad_width"
CONF_PIXEL_MODE = "pixel_mode"
CONF_USE_AXIS_FLIPS = "use_axis_flips"

PIXEL_MODE_24BIT = "24bit"
PIXEL_MODE_18BIT = "18bit"
PIXEL_MODE_16BIT = "16bit"

PIXEL_MODES = {
    PIXEL_MODE_16BIT: 0x55,
    PIXEL_MODE_18BIT: 0x66,
    PIXEL_MODE_24BIT: 0x77,
}

MODE_RGB = "RGB"
MODE_BGR = "BGR"
COLOR_ORDERS = {
    MODE_RGB: ColorOrder.COLOR_ORDER_RGB,
    MODE_BGR: ColorOrder.COLOR_ORDER_BGR,
}

CONF_HSYNC_BACK_PORCH = "hsync_back_porch"
CONF_HSYNC_FRONT_PORCH = "hsync_front_porch"
CONF_HSYNC_PULSE_WIDTH = "hsync_pulse_width"
CONF_VSYNC_BACK_PORCH = "vsync_back_porch"
CONF_VSYNC_FRONT_PORCH = "vsync_front_porch"
CONF_VSYNC_PULSE_WIDTH = "vsync_pulse_width"
CONF_PCLK_FREQUENCY = "pclk_frequency"
CONF_PCLK_INVERTED = "pclk_inverted"
CONF_NATIVE_HEIGHT = "native_height"
CONF_NATIVE_WIDTH = "native_width"

CONF_DE_PIN = "de_pin"
CONF_PCLK_PIN = "pclk_pin"


def power_of_two(value):
    value = cv.int_range(1, 128)(value)
    if value & (value - 1) != 0:
        raise cv.Invalid("value must be a power of two")
    return value


def validate_dimension(rounding):
    def validator(value):
        value = cv.positive_int(value)
        if value % rounding != 0:
            raise cv.Invalid(f"Dimensions and offsets must be divisible by {rounding}")
        return value

    return validator


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
                cv.Optional(CONF_PAD_WIDTH): validate_dimension(rounding),
                cv.Optional(CONF_PAD_HEIGHT): validate_dimension(rounding),
            }
        ),
    )


def map_sequence(value):
    """
    Maps one entry in a sequence to a command and data bytes.
    The format is a repeated sequence of [CMD, <data>] where <data> is s a sequence of bytes. The length is inferred
    from the length of the sequence and should not be explicit.
    A single integer can be provided where there are no data bytes, in which case it is treated as a command.
    A delay can be inserted by specifying "- delay N" where N is in ms
    """
    if isinstance(value, str) and value.lower().startswith("delay "):
        value = value.lower()[6:]
        delay_value = cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(TimePeriod(milliseconds=1), TimePeriod(milliseconds=255)),
        )(value)
        return DELAY_FLAG, delay_value.total_milliseconds
    value = cv.All(cv.ensure_list(cv.int_range(0, 255)), cv.Length(1, 254))(value)
    return tuple(value)


def flatten_sequence(sequence: tuple | list):
    """
    Flatten an init sequence into a single list of bytes.
    :param sequence:  The list of tuples
    :return: a list of bytes
    """
    return sum(
        tuple(
            (x[1], 0xFF) if x[0] == DELAY_FLAG else (x[0], len(x) - 1) + x[1:]
            for x in sequence
        ),
        (),
    )


def delay(ms):
    return DELAY_FLAG, ms


# Generic placeholder model present in every DriverChip registry; skipped when
# choosing a representative model for schema extraction.
_CUSTOM_MODEL = "CUSTOM"


def model_schema_extractor(
    models: dict[str, Any],
    model_schema: Callable[[dict[str, Any]], Any],
    extra: dict[str, Any] | None = None,
) -> Callable[[Callable[[Any], Any]], Callable[[Any], Any]]:
    """
    Decorate a model-driven display CONFIG_SCHEMA so the language-schema dumper
    can extract it.

    The schema is generated per ``model`` at validation time, so the static
    dumper has nothing to walk. When the dumper passes SCHEMA_EXTRACT, resolve a
    representative schema for a real model (the generic "CUSTOM" placeholder
    over-constrains fields like init_sequence) plus any *extra* keys the model
    needs, e.g. a bus mode, and hand that back; runtime validation is untouched.
    """

    def decorate(config_schema: Callable[[Any], Any]) -> Callable[[Any], Any]:
        @schema_extractor("schema")
        @functools.wraps(config_schema)
        def wrapper(config: Any) -> Any:
            if config is not SCHEMA_EXTRACT:
                return config_schema(config)
            names = sorted(models)
            representative = next((n for n in names if n != _CUSTOM_MODEL), names[0])
            schema = model_schema({CONF_MODEL: representative, **(extra or {})})
            if isinstance(schema, vol.All):
                schema = next(
                    (v for v in schema.validators if isinstance(v, vol.Schema)),
                    schema,
                )
            if isinstance(schema, vol.Schema):
                # The resolved schema pins ``model`` to the representative; expose
                # the full model list so the dumped enum offers every model.
                schema = schema.extend(
                    {cv.Required(CONF_MODEL): cv.one_of(*names, upper=True)}
                )
            return schema

        return wrapper

    return decorate


class DriverChip:
    """
    A class representing a MIPI DBI driver chip model.
    The parameters supplied as defaults will be used to provide default values for the display configuration.
    Pass a ``transforms`` set to restrict which axis transforms (mirror_x, mirror_y, swap_xy) the model
    supports; by default all three are available.
    """

    models: dict[str, Self] = {}

    def __init__(
        self,
        name: str,
        initsequence=None,
        **defaults,
    ):
        name = name.upper()
        self.name = name
        self.initsequence = initsequence
        if CONF_NATIVE_WIDTH in defaults:
            if CONF_WIDTH not in defaults:
                defaults[CONF_WIDTH] = (
                    defaults[CONF_NATIVE_WIDTH]
                    - defaults.get(CONF_OFFSET_WIDTH, 0)
                    - defaults.get(CONF_PAD_WIDTH, 0)
                )
            elif defaults[CONF_WIDTH] > defaults[CONF_NATIVE_WIDTH]:
                defaults[CONF_NATIVE_WIDTH] = defaults[CONF_WIDTH]

        else:
            native_width = (
                defaults.get(CONF_WIDTH, 0)
                + defaults.get(CONF_OFFSET_WIDTH, 0)
                + defaults.get(CONF_PAD_WIDTH, 0)
            )
            if native_width != 0:
                defaults[CONF_NATIVE_WIDTH] = native_width
        if CONF_NATIVE_HEIGHT in defaults:
            if CONF_HEIGHT not in defaults:
                defaults[CONF_HEIGHT] = (
                    defaults[CONF_NATIVE_HEIGHT]
                    - defaults.get(CONF_OFFSET_HEIGHT, 0)
                    - defaults.get(CONF_PAD_HEIGHT, 0)
                )
            elif defaults[CONF_HEIGHT] > defaults[CONF_NATIVE_HEIGHT]:
                defaults[CONF_NATIVE_HEIGHT] = defaults[CONF_HEIGHT]
        else:
            native_height = (
                defaults.get(CONF_HEIGHT, 0)
                + defaults.get(CONF_OFFSET_HEIGHT, 0)
                + defaults.get(CONF_PAD_HEIGHT, 0)
            )
            if native_height != 0:
                defaults[CONF_NATIVE_HEIGHT] = native_height
        self.defaults = defaults
        DriverChip.models[name] = self

    @classmethod
    def get_models(cls):
        """
        Return the current set of models and reset the models dictionary.
        """
        models = cls.models
        cls.models = {}
        return models

    def extend(self, name, **kwargs) -> "DriverChip":
        """
        Extend the current model with additional parameters or a modified init sequence.
        Parameters supplied here will override the defaults of the current model.
        if the initsequence is not provided, the current model's initsequence will be used.
        If add_init_sequence is provided, it will be appended to the current initsequence.
        :param name:
        :param kwargs:
        :return:
        """
        initsequence = list(kwargs.pop("initsequence", self.initsequence))
        initsequence.extend(kwargs.pop("add_init_sequence", ()))
        defaults = self.defaults.copy()
        defaults.update(kwargs)
        return self.__class__(name, initsequence=tuple(initsequence), **defaults)

    def get_default(self, key, fallback: Any = False) -> Any:
        return self.defaults.get(key, fallback)

    @property
    def transforms(self) -> set[str]:
        """
        Return the available transforms for this model.
        """
        if (transforms := self.get_default(CONF_TRANSFORMS, None)) is not None:
            return transforms
        if self.get_default("no_transform", False):
            return set()
        if self.get_default(CONF_SWAP_XY) != cv.UNDEFINED:
            return {CONF_MIRROR_X, CONF_MIRROR_Y, CONF_SWAP_XY}
        raise ValueError(
            "Setting 'swap_xy' to 'cv.UNDEFINED' is no longer supported; set 'transforms' instead"
        )

    def has_hardware_transform(self, config) -> bool:
        """
        Check if the model supports hardware transforms for the given configuration.
        """
        return config.get(CONF_TRANSFORM) != CONF_DISABLED and self.transforms == {
            CONF_MIRROR_X,
            CONF_MIRROR_Y,
            CONF_SWAP_XY,
        }

    def option(self, name, fallback=False) -> cv.Optional:
        return cv.Optional(name, default=self.get_default(name, fallback))

    def rotation_as_transform(self, config) -> bool:
        """
        Check if a rotation can be implemented in hardware using the MADCTL register.
        A rotation of 180 is always possible if x and y mirroring are supported, 90 and 270 are possible if the model supports swapping X and Y.
        """
        if config.get(CONF_TRANSFORM) == CONF_DISABLED:
            return False
        transforms = self.transforms
        rotation = config.get(CONF_ROTATION, 0)
        if rotation == 0 or not transforms:
            return False
        if rotation == 180:
            return CONF_MIRROR_X in transforms and CONF_MIRROR_Y in transforms
        if rotation == 90:
            return CONF_SWAP_XY in transforms and CONF_MIRROR_X in transforms
        return CONF_SWAP_XY in transforms and CONF_MIRROR_Y in transforms

    def get_dimensions(
        self, config, swap: bool = True
    ) -> tuple[int, int, int, int, int, int]:
        """
        Return the dimensions of the current model.
        :param config: The current configuration
        :param swap: If width/height should be swapped when axes are swapped.
        :return: A tuple (width, height, offset_width, offset_height, pad_width, pad_height).
        """

        transform = self.get_transform(config)
        if CONF_DIMENSIONS in config:
            # Explicit dimensions, just use as is
            dimensions = config[CONF_DIMENSIONS]
            if isinstance(dimensions, dict):
                native_width = self.get_default(CONF_NATIVE_WIDTH, 0)
                native_height = self.get_default(CONF_NATIVE_HEIGHT, 0)
                if transform.get(CONF_SWAP_XY) is True:
                    native_width, native_height = native_height, native_width
                width = dimensions[CONF_WIDTH]
                height = dimensions[CONF_HEIGHT]
                offset_width = dimensions[CONF_OFFSET_WIDTH]
                offset_height = dimensions[CONF_OFFSET_HEIGHT]
                if CONF_PAD_WIDTH in dimensions:
                    pad_width = dimensions[CONF_PAD_WIDTH]
                    native_width = width + offset_width + pad_width
                elif native_width == 0:
                    pad_width = 0
                    native_width = width + offset_width
                else:
                    pad_width = native_width - width - offset_width
                if CONF_PAD_HEIGHT in dimensions:
                    pad_height = dimensions[CONF_PAD_HEIGHT]
                    native_height = height + offset_height + pad_height
                elif native_height == 0:
                    pad_height = 0
                    native_height = height + offset_height
                else:
                    pad_height = native_height - height - offset_height
                if (
                    pad_width + offset_width >= native_width
                    or pad_height + offset_height >= native_height
                ):
                    raise cv.Invalid("Dimensions exceed native size", [CONF_DIMENSIONS])
                if pad_width < 0 or pad_height < 0:
                    raise cv.Invalid("Invalid offsets", [CONF_DIMENSIONS])

                return width, height, offset_width, offset_height, pad_width, pad_height

            # Must be a tuple
            width, height = dimensions
            return width, height, 0, 0, 0, 0

        # Default dimensions, use model defaults

        width = self.get_default(CONF_WIDTH)
        height = self.get_default(CONF_HEIGHT)
        native_width = self.get_default(CONF_NATIVE_WIDTH, 0)
        native_height = self.get_default(CONF_NATIVE_HEIGHT, 0)
        offset_width = self.get_default(CONF_OFFSET_WIDTH, 0)
        offset_height = self.get_default(CONF_OFFSET_HEIGHT, 0)
        pad_width = self.get_default(
            CONF_PAD_WIDTH, native_width - width - offset_width
        )
        pad_height = self.get_default(
            CONF_PAD_HEIGHT, native_height - height - offset_height
        )

        if pad_width < 0 or pad_height < 0:
            raise cv.Invalid("Offsets exceed native size", [CONF_DIMENSIONS])

        # if mirroring axes and there are offsets, also mirror the offsets to cater for situations where
        # the offset is asymmetric
        if transform.get(CONF_MIRROR_X):
            offset_width, pad_width = pad_width, offset_width
        if transform.get(CONF_MIRROR_Y):
            offset_height, pad_height = pad_height, offset_height
        # Swap default dimensions if swap_xy is set, or if rotation is 90/270, and we are not using a buffer
        if swap and transform.get(CONF_SWAP_XY) is True:
            width, height = height, width
            offset_height, offset_width = offset_width, offset_height
            pad_width, pad_height = pad_height, pad_width
        return width, height, offset_width, offset_height, pad_width, pad_height

    def get_base_transform(self, config):
        transform = config.get(
            CONF_TRANSFORM,
            {
                CONF_MIRROR_X: self.get_default(CONF_MIRROR_X),
                CONF_MIRROR_Y: self.get_default(CONF_MIRROR_Y),
                CONF_SWAP_XY: self.get_default(CONF_SWAP_XY),
            },
        )
        if isinstance(transform, dict):
            return transform

        # Transform is disabled
        return {
            CONF_MIRROR_X: False,
            CONF_MIRROR_Y: False,
            CONF_SWAP_XY: False,
            CONF_TRANSFORM: False,
        }

    def get_transform(self, config) -> dict[str, bool]:
        transform = self.get_base_transform(config)
        # Can we use the MADCTL register to set the rotation?
        transform[CONF_TRANSFORM] = self.rotation_as_transform(config)
        return transform

    def transform_schema(self):
        """
        Build the schema for the ``transform`` config option of this model.

        Each transform the model supports is a required boolean. A transform the model does not
        support may be omitted or set to ``false``; setting it to ``true`` reports a clear error
        naming the unsupported transform instead of a generic "extra keys not allowed".
        """
        supported = self.transforms

        def unsupported(name):
            def validator(value):
                if cv.boolean(value):
                    raise cv.Invalid(f"'{name}' is not supported by this model")
                return False

            return validator

        schema = {}
        for name in ALL_TRANSFORMS:
            if name in supported:
                schema[cv.Required(name)] = cv.boolean
            else:
                schema[cv.Optional(name, default=False)] = unsupported(name)
        return cv.Any(cv.Schema(schema), cv.one_of(CONF_DISABLED, lower=True))

    def get_madctl(self, transform: dict, config: dict) -> int:
        """
        Convert a transform to MADCTL bits
        :param transform: The transform dict
        :param use_flip: Whether to use axis flips
        :return: MADCTL value
        """
        use_flip = config.get(CONF_USE_AXIS_FLIPS, False)
        madctl = MADCTL_FLIP_FLAG if use_flip else 0
        if transform[CONF_MIRROR_X]:
            madctl |= MADCTL_XFLIP if use_flip else MADCTL_MX
        if transform[CONF_MIRROR_Y]:
            madctl |= MADCTL_YFLIP if use_flip else MADCTL_MY
        if transform.get(CONF_SWAP_XY) is True:  # Exclude Undefined
            madctl |= MADCTL_MV
        if config[CONF_COLOR_ORDER] == MODE_BGR:
            madctl |= MADCTL_BGR
        return madctl

    def add_madctl(self, sequence: list, config: dict):
        # Add the MADCTL command to the sequence based on the base configuration.
        # Rotation is not applied here, it will be done at runtime.
        transform = self.get_transform(config)
        madctl = self.get_madctl(transform, config)
        sequence.append((MADCTL, madctl & 0xFF))

    def skip_command(self, command: str):
        """
        Allow suppressing a standard command in the init sequence.
        """
        return self.get_default(f"no_{command.lower()}", False)

    def get_sequence(self, config, add_madctl=True, add_reset=False) -> tuple[int, ...]:
        """
        Create the init sequence for the display.
        Use the default sequence from the model, if any, and append any custom sequence provided in the config.
        Append SLPOUT (if not already in the sequence) and DISPON to the end of the sequence
        MADCTL will be set if add_madctl is True
        If add_reset is True, a reset is prepended: a software reset when no reset pin
        is configured (and the model doesn't skip it), followed by a settling delay that
        both a software and a hardware reset require.
        Returns the init sequence
        """
        sequence = list(self.initsequence or ())
        custom_sequence = config.get(CONF_INIT_SEQUENCE, [])
        sequence.extend(custom_sequence)
        # Ensure each command is a tuple
        sequence = [x if isinstance(x, tuple) else (x,) for x in sequence]

        if add_reset:
            reset: list = []
            # A software reset is only needed when there is no hardware reset pin.
            if CONF_RESET_PIN not in config and not self.skip_command("SWRESET"):
                reset.append((SWRESET,))
            # Both a software and a hardware reset need a settling delay before further commands.
            reset.append(delay(10))
            sequence = reset + sequence

        # Set pixel format if not already in the custom sequence
        pixel_mode = config[CONF_PIXEL_MODE]
        if not isinstance(pixel_mode, int):
            if not pixel_mode.endswith("bit"):
                pixel_mode = f"{pixel_mode}bit"
            pixel_mode = PIXEL_MODES[pixel_mode]
        sequence.append((PIXFMT, pixel_mode))

        if self.rotation_as_transform(config):
            LOGGER.info("Using hardware transform to implement rotation")
        if add_madctl:
            self.add_madctl(sequence, config)
        if config[CONF_INVERT_COLORS]:
            sequence.append((INVON,))
        else:
            sequence.append((INVOFF,))
        if brightness := config.get(CONF_BRIGHTNESS, self.get_default(CONF_BRIGHTNESS)):
            sequence.append((BRIGHTNESS, brightness))
        # Add a SLPOUT command if required.
        if not self.skip_command("SLPOUT"):
            # A zero delay will delay until 120ms after reset
            sequence.append(delay(0))
            sequence.append((SLPOUT,))
            sequence.append(delay(10))
        sequence.append((DISPON,))
        # Add a delay here because additional commands may be added after this at runtime.
        sequence.append(delay(10))

        # Flatten the sequence into a list of bytes, with the length of each command
        # or the delay flag inserted where needed
        return flatten_sequence(sequence)

    def check_requirements(self) -> None:
        """
        Raise a friendly error if any component this model requires is not configured.

        This runs during schema validation (before ID references are resolved) so that a
        model whose default pins live on a pin expander reports the missing expander clearly
        instead of a cryptic "Couldn't find ID" from the unresolved pin reference.
        """
        requirements = self.get_default("requires", set())
        if not requirements:
            return
        # ``raw_config`` is populated before any component schema runs during a real
        # validation, so presence of a required component is simply a top-level key.
        # When it is absent (e.g. a unit test that invokes the schema directly) there
        # is no config to check against, so skip.
        global_config = CORE.raw_config
        if global_config is None:
            return
        missing = {x for x in requirements if x not in global_config}
        if missing:
            reqstr = ", ".join(f"'{x}'" for x in sorted(missing))
            raise cv.Invalid(
                f"{self.name} requires component{'s' if len(missing) > 1 else ''} {reqstr} to be configured"
            )


def requires_buffer(config) -> bool:
    """
    Check if the display configuration requires a buffer. It will do so if any drawing methods are configured.
    :param config:
    :return:  True if a buffer is required, False otherwise
    """
    return any(
        config.get(key) for key in (CONF_LAMBDA, CONF_PAGES, CONF_SHOW_TEST_CARD)
    )


def get_color_depth(config) -> int:
    """
    Get the color depth in bits from the configuration.
    """
    return int(config[CONF_COLOR_DEPTH].removesuffix("bit"))

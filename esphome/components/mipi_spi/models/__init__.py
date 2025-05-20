from esphome.components.spi import TYPE_OCTAL, TYPE_QUAD, TYPE_SINGLE
import esphome.config_validation as cv
from esphome.const import CONF_HEIGHT, CONF_OFFSET_HEIGHT, CONF_OFFSET_WIDTH, CONF_WIDTH

from .. import CONF_NATIVE_HEIGHT, CONF_NATIVE_WIDTH

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

DELAY_FLAG = 0xFFF  # Special flag to indicate a delay


def delay(ms):
    return DELAY_FLAG, ms


class DriverChip:
    models = {}

    def __init__(
        self,
        name: str,
        modes=(TYPE_SINGLE, TYPE_QUAD, TYPE_OCTAL),
        initsequence=None,
        **defaults,
    ):
        name = name.upper()
        self.name = name
        self.modes = modes
        self.initsequence = initsequence
        self.defaults = defaults
        DriverChip.models[name] = self

    def extend(self, name, **kwargs):
        defaults = self.defaults.copy()
        if (
            CONF_WIDTH in defaults
            and CONF_OFFSET_WIDTH in kwargs
            and CONF_NATIVE_WIDTH not in defaults
        ):
            defaults[CONF_NATIVE_WIDTH] = defaults[CONF_WIDTH]
        if (
            CONF_HEIGHT in defaults
            and CONF_OFFSET_HEIGHT in kwargs
            and CONF_NATIVE_HEIGHT not in defaults
        ):
            defaults[CONF_NATIVE_HEIGHT] = defaults[CONF_HEIGHT]
        defaults.update(kwargs)
        return DriverChip(name, self.modes, initsequence=self.initsequence, **defaults)

    def get_default(self, key, fallback=False):
        return self.defaults.get(key, fallback)

    def option(self, name, fallback=False):
        return cv.Optional(name, default=self.get_default(name, fallback))

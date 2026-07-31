import esphome.config_validation as cv
from esphome.const import CONF_BUSY_PIN

from . import EpaperModel


class Waveshare3P97InG(EpaperModel):
    def option(self, name, fallback=cv.UNDEFINED) -> cv.Optional | cv.Required:
        if name == CONF_BUSY_PIN:
            return cv.Required(name)
        return super().option(name, fallback)


# Vendor init derived from the Waveshare 3.97inch E-Paper Display (G) sample.
# The 0x61 payload is deliberately 800x680; it must not be derived from the
# logical 800x480 display dimensions.
Waveshare3P97InG(
    "waveshare-3.97in-g",
    "EPaperWaveshare3P97InG",
    width=800,
    height=480,
    minimum_update_interval="30s",
    data_rate="10MHz",
    initsequence=(
        (0x00, 0x2B, 0x29),
        (0x06, 0x0F, 0x8B, 0x93, 0xC1),
        (0x50, 0x37),
        (0x30, 0x08),
        (0x61, 0x03, 0x20, 0x02, 0xA8),
        (0x62, 0x76, 0x76, 0x76, 0x5A, 0x9D, 0x8A, 0x76, 0x62),
        (0x65, 0x00, 0x00, 0x00, 0x00),
        (0xE0, 0x10),
        (0xE7, 0xA4),
        (0xE9, 0x01),
        (0xEF, 0x01),
        (0xF6, 0x20),
        (0xEF, 0x00),
        (0xE0, 0x12),
        (0xE6, 92),
        (0xA5, 0x00),
    ),
)

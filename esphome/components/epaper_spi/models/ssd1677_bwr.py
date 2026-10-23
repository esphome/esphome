"""Black/White/Red e-paper displays using the SSD1677 controller.

Supported models:
- waveshare-7.5in-hd-b: 880x528 pixels (Waveshare 7.5" HD (B), EPD_7in5b_HD)

The controller scans offset_height + height gates, and the panel is on the last height of them;
offset_height (under dimensions) is a property of the panel.
"""

import esphome.config_validation as cv
from esphome.const import CONF_BUSY_PIN, CONF_OFFSET_HEIGHT, CONF_RESET_PIN

from . import EpaperModel


class SSD1677BWR(EpaperModel):
    """EpaperModel class for Black/White/Red displays using the SSD1677 controller."""

    source_files = ("epaper_weact_3c.cpp", "epaper_spi_ssd1677_bwr.cpp")

    def __init__(self, name: str, **defaults: object) -> None:
        defaults.setdefault(CONF_OFFSET_HEIGHT, 0)
        super().__init__(name, "EPaperSSD1677BWR", **defaults)

    def option(
        self, name: str, fallback: object = cv.UNDEFINED
    ) -> cv.Optional | cv.Required:
        if name in (CONF_RESET_PIN, CONF_BUSY_PIN):
            return cv.Required(name)
        return super().option(name, fallback)

    def get_constructor_args(self, config) -> tuple:
        return (self.get_offset_height(config),)

    def get_init_sequence(self, config):
        """Follow Waveshare's EPD_7in5b_HD init, less the steps the driver does itself.

        The RAM window is set before each push, the RAM fills (0x46, 0x47) are not needed as both
        planes are always written in full, and the full refresh (0x22 0xF7) loads the waveform.
        """
        _, height = self.get_dimensions(config)
        last_gate = self.get_offset_height(config) + height - 1
        return (
            (0x0C, 0xAE, 0xC7, 0xC3, 0xC0, 0x40),  # Booster soft start
            # Driver output control: gate count, scanned bottom to top
            (0x01, last_gate & 0xFF, last_gate >> 8, 0x01),
            (0x11, 0x01),  # Data entry mode: X increments, Y decrements
            (0x3C, 0x01),  # Border waveform
            (0x18, 0x80),  # Internal temperature sensor
        )


# Model: Waveshare 7.5" HD (B) - 880x528 pixels, SSD1677 controller
# The panel is on the last 528 of the controller's 680 gates. Waveshare's code sets 688 gates (0x2AF),
# more than the controller has, which shows the image a few rows too low.
SSD1677BWR(
    "waveshare-7.5in-hd-b",
    width=880,
    height=528,
    offset_height=152,
)

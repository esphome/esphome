"""UC8179-based Black/White/Red e-paper displays.

Supported models:
- 7.5in-bv3-bwr-xsrupb: 800x480 pixels (7.5" V3 BWR XSRUPB 2025 panel)

These displays use the UC8179 controller with separate B/W and Red data planes.
Commands 0x10 (B/W) and 0x13 (Red) are used for data transmission.
"""

from . import EpaperModel

import esphome.config_validation as cv

CONF_INVERT_RED = "invert_red"


class UC8179BWR(EpaperModel):
    """EpaperModel class for UC8179-based Black/White/Red displays."""

    def __init__(self, name, **defaults):
        super().__init__(name, "EPaperUC8179BWR", **defaults)

    def get_init_sequence(self, config):
        """Generate initialization sequence for UC8179 BWR displays."""
        width, height = self.get_dimensions(config)
        return (
            # Panel setting: BWR, LUT from OTP
            (0x00, 0x0F),
            # VCOM and data interval: VBD=01(white border), DDX=11, CDI=0111
            (0x50, 0x77),
            # Resolution
            (0x61, width // 256, width % 256, height // 256, height % 256),
        )

    def get_config_schema(self) -> dict:
        return {
            cv.Optional(
                CONF_INVERT_RED,
                default=self.get_default(CONF_INVERT_RED, False),
            ): cv.boolean,
        }

    def get_constructor_args(self, config) -> tuple:
        return (config.get(CONF_INVERT_RED, self.get_default(CONF_INVERT_RED, False)),)


uc8179bwr = UC8179BWR("uc8179bwr")

# Model: 7.5" V3 BWR XSRUPB 2025 - 800x480 pixels, UC8179 controller
uc8179bwr.extend(
    "7.5IN-BV3-BWR-XSRUPB",
    width=800,
    height=480,
    invert_red=True,
    reset_duration="200ms",
)

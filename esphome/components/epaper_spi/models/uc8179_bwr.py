"""UC8179-based Black/White/Red e-paper displays.

Supported models:
- 7.5in-bv3-bwr-xsrupb: 800x480 pixels (7.5" V3 BWR XSRUPB 2025 panel)

These displays use the UC8179 controller with separate B/W and Red data planes.
Commands 0x10 (B/W) and 0x13 (Red) are used for data transmission.
"""

from typing import Any

import esphome.config_validation as cv
from esphome.types import ConfigType

from . import EpaperModel

CONF_INVERT_RED = "invert_red"


class UC8179BWR(EpaperModel):
    """EpaperModel class for UC8179-based Black/White/Red displays."""

    def __init__(self, name: str, **defaults: Any) -> None:
        super().__init__(name, "EPaperUC8179BWR", **defaults)

    def get_init_sequence(self, config: ConfigType) -> tuple:
        """Generate initialization sequence for UC8179 BWR displays."""
        width, height = self.get_dimensions(config)
        return (
            # Panel setting: BWR, LUT from OTP
            (0x00, 0x0F),
            # VCOM and data interval setting: DDX=11, which needs invert_red
            (0x50, 0x77),
            # Resolution
            (0x61, width // 256, width % 256, height // 256, height % 256),
        )

    def get_config_options(self) -> dict:
        return {
            cv.Optional(
                CONF_INVERT_RED,
                default=self.get_default(CONF_INVERT_RED, True),
            ): cv.boolean,
        }

    def get_constructor_args(self, config: ConfigType) -> tuple:
        return (config[CONF_INVERT_RED],)


uc8179bwr = UC8179BWR("uc8179bwr", minimum_update_interval="30s")

# Model: 7.5" V3 BWR XSRUPB 2025 - 800x480 pixels, UC8179 controller
uc8179bwr.extend(
    "7.5IN-BV3-BWR-XSRUPB",
    width=800,
    height=480,
    reset_duration="200ms",
)

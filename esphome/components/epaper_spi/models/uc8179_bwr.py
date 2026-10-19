"""UC8179-based Black/White/Red e-paper displays.

Supported models:
- XSRUPB-DEPG0750RWF86BF30-7.5in-BWR: 800x480 pixels (7.5" V3 BWR XSRUPB 2025 panel)

These displays use the UC8179 controller and the EPaperWaveshareBWR driver, with this
init sequence and red data always sent inverted.
"""

from typing import Any

from esphome.types import ConfigType

from . import EpaperModel


class UC8179BWR(EpaperModel):
    """EpaperModel class for UC8179-based Black/White/Red displays."""

    def __init__(self, name: str, **defaults: Any) -> None:
        super().__init__(name, "EPaperWaveshareBWR", **defaults)

    def get_init_sequence(self, config: ConfigType) -> tuple:
        """Generate initialization sequence for UC8179 BWR displays."""
        width, height = self.get_dimensions(config)
        return (
            # Panel setting: BWR, LUT from OTP
            (0x00, 0x0F),
            # VCOM and data interval setting: DDX=11, which needs the red data inverted
            (0x50, 0x77),
            # Resolution
            (0x61, width // 256, width % 256, height // 256, height % 256),
        )

    def get_constructor_args(self, config: ConfigType) -> tuple:
        return (True,)  # invert_red, required by DDX=11 above


uc8179bwr = UC8179BWR("uc8179bwr", minimum_update_interval="30s")

# Model: 7.5" V3 BWR XSRUPB 2025 - 800x480 pixels, UC8179 controller
uc8179bwr.extend(
    "XSRUPB-DEPG0750RWF86BF30-7.5in-BWR",
    width=800,
    height=480,
    reset_duration="200ms",
)

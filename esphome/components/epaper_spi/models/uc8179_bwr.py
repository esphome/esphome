"""UC8179-based Black/White/Red e-paper displays.

Supported models:
- 7.5in-bv3-bwr-xsrupb: 800x480 pixels (7.5" V3 BWR XSRUPB 2025 panel)

These displays use the UC8179 controller with separate B/W and Red data planes.
Commands 0x10 (B/W) and 0x13 (Red) are used for data transmission.
"""

from . import EpaperModel


class UC8179BWR(EpaperModel):
    """EpaperModel class for UC8179-based Black/White/Red displays."""

    def __init__(self, name, **defaults):
        super().__init__(name, "EPaperUC8179BWR", **defaults)

    def get_init_sequence(self, config):
        """Generate initialization sequence for UC8179 BWR displays."""
        width, height = self.get_dimensions(config)
        return (
            (0x00, 0x0F),                            # Panel Setting
            (0x50, 0xF5, 0x07),                      # VCOM/Data Interval (both bytes)
            # Resolution
            (0x61, width // 256, width % 256, height // 256, height % 256),
        )

uc8179bwr = UC8179BWR("uc8179bwr")

# Model: 7.5" V3 BWR XSRUPB 2025 - 800x480 pixels, UC8179 controller
uc8179bwr.extend(
    "7.5IN-BV3-BWR-XSRUPB",
    width=800,
    height=480,
    reset_duration="200ms",
)

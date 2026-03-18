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
        """Generate initialization sequence for UC8179 BWR displays.

        Note: Resolution command (0x61) is sent automatically after this
        sequence using the configured width/height values.
        """
        return (
            # Panel setting: BWR, LUT from OTP
            (0x00, 0x0F),
            # VCOM and data interval: white border (B/W=LUTWK, Red=HiZ), CDI
            (0x50, 0x77, 0x07),
        )


# Model: 7.5" V3 BWR XSRUPB 2025 - 800x480 pixels, UC8179 controller
UC8179BWR(
    "7.5IN-BV3-BWR-XSRUPB",
    width=800,
    height=480,
    invert_red=True,
)

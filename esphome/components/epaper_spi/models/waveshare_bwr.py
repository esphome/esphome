"""Waveshare Black/White/Red e-paper displays using UC8179 controller.

Supported models:
- waveshare-7.5in-bv2-bwr: 800x480 pixels (7.5" BWR display, EDP_7in5b_V2)

These displays use the UC8179 controller. Panel configuration is sent during
the INITIALISE state. Power-on is handled in the POWER_ON state, after data
transfer, so the state machine's built-in busy wait covers the power-on delay.
"""

from . import EpaperModel


class WaveshareBWR(EpaperModel):
    """EpaperModel class for Waveshare Black/White/Red displays using UC8179 controller."""

    def __init__(self, name, **defaults):
        super().__init__(name, "EPaperWaveshareBWR", **defaults)

    def get_init_sequence(self, config):
        """Generate initialization sequence for UC8179 BWR displays.

        Panel configuration only — power-on is handled separately in power_on()
        after data transfer, with the state machine busy-waiting before refresh.
        """
        width, height = self.get_dimensions(config)
        return (
            # PANEL SETTING (KWR mode)
            (0x00, 0x0F),
            # RESOLUTION SETTING (width x height)
            (
                0x61,
                (width >> 8) & 0xFF,
                width & 0xFF,
                (height >> 8) & 0xFF,
                height & 0xFF,
            ),
            # DUAL SPI MODE (disabled)
            (0x15, 0x00),
            # VCOM AND DATA INTERVAL SETTING
            (0x50, 0x11, 0x07),
            # TCON SETTING
            (0x60, 0x22),
            # RESOLUTION GATE SETTING
            (0x65, 0x00, 0x00, 0x00, 0x00),
        )


# Model: Waveshare 7.5" V2 BWR (EDP_7in5b_V2) — 800x480, UC8179 controller
WaveshareBWR(
    "waveshare-7.5in-bv2-bwr",
    width=800,
    height=480,
    data_rate="10MHz",
    minimum_update_interval="30s",
)

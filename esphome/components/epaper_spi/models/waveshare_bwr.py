"""Black/White/Red e-paper displays using UC8179 controller.

Supported models:
- waveshare-7.5in-bv2-bwr: 800x480 pixels (7.5" BWR display, EPD_7in5b_V2)
- XSRUPB-7.5in-bwr: 800x480 pixels (7.5" V3 BWR XSRUPB 2025 panel)

These displays use the UC8179 controller. Power and panel configuration are sent
during the INITIALISE state. Power-on is handled in the POWER_ON state, after data
transfer, so the state machine's built-in busy wait covers the power-on delay.
"""

import esphome.config_validation as cv
from esphome.const import CONF_DIMENSIONS
from esphome.types import ConfigType

from . import EpaperModel, ultrachip_resolution

# UC8179 resolution setting (0x61) limits; sources are set in steps of 8
MAX_SOURCES = 800
MAX_GATES = 600


class WaveshareBWR(EpaperModel):
    """EpaperModel class for Waveshare Black/White/Red displays using UC8179 controller."""

    def __init__(self, name, **defaults):
        defaults.setdefault("minimum_update_interval", "30s")
        defaults.setdefault("width_multiple", 8)
        super().__init__(name, "EPaperWaveshareBWR", **defaults)

    def validate_config(self, config: ConfigType) -> ConfigType:
        width, height = self.get_dimensions(config)
        if width > MAX_SOURCES or height > MAX_GATES:
            raise cv.Invalid(
                f"{self.name} supports at most {MAX_SOURCES}x{MAX_GATES} pixels (width x height)",
                path=[CONF_DIMENSIONS],
            )
        return config

    def get_init_sequence(self, config):
        """Generate initialization sequence for UC8179 BWR displays.

        Power and panel configuration only — power-on is handled separately in
        power_on() after data transfer, with the state machine busy-waiting before refresh.
        """
        width, height = self.get_dimensions(config)
        return (
            # PANEL SETTING (KWR mode)
            (0x00, 0x0F),
            # RESOLUTION SETTING (width x height)
            ultrachip_resolution(width, height),
            # DUAL SPI MODE (disabled)
            (0x15, 0x00),
            # VCOM AND DATA INTERVAL SETTING
            (0x50, 0x11, 0x07),
            # TCON SETTING
            (0x60, 0x22),
            # RESOLUTION GATE SETTING
            (0x65, 0x00, 0x00, 0x00, 0x00),
            # POWER SETTING
            (0x01, 0x07, 0x17, 0x3F, 0x3F),
        )


# Model: Waveshare 7.5" V2 BWR (EPD_7in5b_V2) — 800x480, UC8179 controller
WaveshareBWR(
    "waveshare-7.5in-bv2-bwr",
    width=800,
    height=480,
    data_rate="10MHz",
)


class XsrupbBWR(WaveshareBWR):
    """WaveshareBWR with the XSRUPB init sequence, which sets DDX=11, so the red data is sent inverted."""

    def get_init_sequence(self, config: ConfigType) -> tuple:
        width, height = self.get_dimensions(config)
        return (
            # Panel setting: BWR, LUT from OTP
            (0x00, 0x0F),
            # VCOM and data interval setting: BDV=11 (border uses LUTBD), DDX=11, which needs the red data inverted
            (0x50, 0x33),
            ultrachip_resolution(width, height),
            # Power setting, as for waveshare-7.5in-bv2-bwr: internal power, VGH/VGL=±20V, VDH/VDL=±15V
            (0x01, 0x07, 0x17, 0x3F, 0x3F),
        )

    def get_constructor_args(self, config: ConfigType) -> tuple:
        return (True,)  # invert_red, required by DDX=11 above


# Model: 7.5" V3 BWR XSRUPB 2025 - 800x480 pixels, UC8179 controller
XsrupbBWR(
    "XSRUPB-7.5in-bwr",
    width=800,
    height=480,
)

"""Monochrome e-paper displays using the UC8179 controller.

Supported models:
- waveshare-7.5in-v2: 7.5" mono display, 800x480 pixels (EPD_7in5_V2)
- seeed-reterminal-e1001: Seeed reTerminal E1001, which uses the same
  7.5" 800x480 panel on an integrated ESP32-S3 board

Panel configuration and power-on (0x04) are both sent during the INITIALISE
state; the state machine's built-in busy wait then covers the power-on delay
before the waveform/mode registers and image data are transferred.

These displays support fast full and partial refresh: set ``full_update_every``
greater than 1 to enable it. Every ``full_update_every``-th update is a fast
full refresh, with partial refreshes in between.

- waveshare-7.5in-v2-gray4: the same 7.5" panel in four-level grayscale, using
  LUTs uploaded at init. Full refresh only.
"""

from typing import Any

from esphome.const import CONF_DATA_RATE

from . import EpaperModel, ultrachip_resolution


class UC8179(EpaperModel):
    """EpaperModel class for monochrome displays using the UC8179 controller."""

    def __init__(
        self,
        name: str,
        class_name: str = "EPaperUC8179",
        data_rate: str = "10MHz",
        **defaults: Any,
    ) -> None:
        defaults.setdefault(CONF_DATA_RATE, data_rate)
        defaults.setdefault("partial_update", True)
        super().__init__(name, class_name=class_name, **defaults)

    def get_init_sequence(self, config: dict) -> tuple:
        """Generate the initialization sequence for UC8179 mono displays.

        Panel configuration only — the driver appends power-on (0x04) at the
        end of the INITIALISE state, and the state machine busy-waits for it
        to complete before the data transfer starts.
        """
        width, height = self.get_dimensions(config)
        return (
            # POWER SETTING
            (0x01, 0x07, 0x07, 0x3F, 0x3F),
            # BOOSTER SOFT START
            (0x06, 0x17, 0x17, 0x28, 0x17),
            # PANEL SETTING (black/white mode, LUT from OTP)
            (0x00, 0x1F),
            # RESOLUTION SETTING (width x height)
            ultrachip_resolution(width, height),
            # DUAL SPI MODE (disabled)
            (0x15, 0x00),
            # VCOM AND DATA INTERVAL SETTING
            (0x50, 0x10, 0x07),
            # TCON SETTING
            (0x60, 0x22),
        )


class UC8179Gray4(UC8179):
    """Four-level grayscale on the UC8179 7.5" V2 panel, full refresh only."""

    # fmt: off
    def get_init_sequence(self, config: dict) -> tuple:
        """Panel configuration and the grayscale LUTs, sent once the driver has powered the panel on."""
        width, height = self.get_dimensions(config)
        return (
            (0x00, 0x3F),  # PANEL SETTING (LUT from registers)
            ultrachip_resolution(width, height),
            (0x15, 0x00),  # DUAL SPI MODE (disabled)
            (0x50, 0x00, 0x07),  # VCOM AND DATA INTERVAL SETTING, replaced after power-on
            (0x60, 0x22),  # TCON SETTING
            (0x82, 0x12),  # VCOM DC SETTING
            (0x20,) + _gray4_lut(0x00, 0x12, 0x04),  # LUT VCOM
            (0x21,) + _gray4_lut(0x80, 0x12, 0x04),  # LUT WW
            (0x22,) + _gray4_lut(0x90, 0x11, 0x05),  # LUT BW
            (0x23,) + _gray4_lut(0x60, 0x11, 0x05),  # LUT WB
            (0x24,) + _gray4_lut(0x40, 0x12, 0x04),  # LUT BB
            (0x25,) + _gray4_lut(0xA0, 0x12, 0x04),  # LUT BORDER
            (0xE0, 0x00),  # CASCADE SETTING
            (0x41, 0x00),  # TEMPERATURE SENSOR (internal)
        )
    # fmt: on


def _gray4_lut(select: int, frames_a: int, frames_b: int) -> tuple:
    """One 42-byte LUT: a single phase group, the rest zero."""
    return (select, frames_a, frames_b, 0x01, 0x00, 0x01) + (0x00,) * 36


uc8179 = UC8179("uc8179")

# Waveshare 7.5" V2 mono (EPD_7in5_V2) — 800x480, UC8179 controller
waveshare_7_5_v2 = uc8179.extend(
    "waveshare-7.5in-v2",
    width=800,
    height=480,
)

# Seeed reTerminal E1001 — 7.5" mono e-paper (800x480), same panel as the
# Waveshare 7.5" V2, driven by an integrated ESP32-S3 board
waveshare_7_5_v2.extend(
    "seeed-reterminal-e1001",
    cs_pin=10,
    dc_pin=11,
    reset_pin=12,
    busy_pin={
        "number": 13,
        "inverted": True,
        "mode": {
            "input": True,
            "pullup": True,
        },
    },
)

# Waveshare 7.5" V2 (EPD_7in5_V2) in four-level grayscale
UC8179Gray4(
    "waveshare-7.5in-v2-gray4",
    class_name="EPaperUC8179Gray4",
    partial_update=False,
    # each plane byte is built from two whole buffer bytes
    width_multiple=8,
    width=800,
    height=480,
)

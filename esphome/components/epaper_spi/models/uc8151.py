"""Black/white/red e-paper displays using the UltraChip UC8151D controller.

Supported models:
- uc8151: the controller on its own; dimensions and pins must be configured
- waveshare-2.13in-bv3: 104x212 pixels (2.13" B V3, EPD_2in13b_V3)

The busy pin of these displays is low while busy, so configure it with ``inverted: true``.
"""

from typing import Any

from . import EpaperModel


class UC8151(EpaperModel):
    """EpaperModel class for black/white/red displays using the UC8151D controller."""

    def __init__(self, name: str, **defaults: Any) -> None:
        super().__init__(name, "EPaperWaveshareBWR", **defaults)

    def get_init_sequence(self, config: dict) -> tuple:
        width, height = self.get_dimensions(config)
        return (
            # PANEL SETTING (black/white/red mode, LUT from OTP)
            (0x00, 0x0F, 0x89),
            # RESOLUTION SETTING: sources, then gates
            (0x61, width, height >> 8, height & 0xFF),
            # VCOM AND DATA INTERVAL SETTING
            (0x50, 0x57),
        )


uc8151 = UC8151(
    "uc8151",
    data_rate="10MHz",
    minimum_update_interval="15s",
    width_multiple=8,
)

uc8151.extend(
    "waveshare-2.13in-bv3",
    width=104,
    height=212,
)

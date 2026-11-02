"""E-paper displays using the UltraChip UC8151D controller.

Supported black/white/red models:
- uc8151: the controller on its own; dimensions and pins must be configured
- waveshare-2.13in-bv3: 104x212 pixels (2.13" B V3, EPD_2in13b_V3)

Supported black/white models, with partial updates when ``full_update_every`` is greater than 1:
- goodisplay-gdew029t5-2.9: Good Display GDEW029T5, 2.9" 128x296 flexible panel; replaces the
  waveshare_epaper model gdew029t5. Also sold by Seeed as SKU 104990852 (seeed-104990852).

The busy pin of these displays is low while busy, so configure it with ``inverted: true``.
"""

from typing import Any

import esphome.config_validation as cv
from esphome.const import CONF_DIMENSIONS
from esphome.types import ConfigType

from . import EpaperModel

MAX_SOURCES = 160
MAX_GATES = 296


class UC8151(EpaperModel):
    """EpaperModel class for black/white/red displays using the UC8151D controller."""

    source_files = ("epaper_spi_bwr.cpp", "epaper_waveshare_bwr.cpp")

    def __init__(
        self, name: str, class_name: str = "EPaperWaveshareBWR", **defaults: Any
    ) -> None:
        super().__init__(name, class_name, **defaults)

    def validate_config(self, config: ConfigType) -> ConfigType:
        width, height = self.get_dimensions(config)
        if width > MAX_SOURCES or height > MAX_GATES:
            raise cv.Invalid(
                f"{self.name} supports at most {MAX_SOURCES}x{MAX_GATES} pixels (width x height)",
                path=[CONF_DIMENSIONS],
            )
        return config

    def get_init_sequence(self, config: dict) -> tuple:
        width, height = self.get_dimensions(config)
        return (
            # PANEL SETTING (black/white/red mode, LUT from OTP)
            (0x00, 0x0F, 0x89),
            # RESOLUTION SETTING: sources, then gates
            (0x61, width, height >> 8, height & 0xFF),
            # VCOM AND DATA INTERVAL SETTING; 0x57, not Waveshare's 0x77, so that 1 in DTM2 is red
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


class UC8151Mono(UC8151):
    """EpaperModel class for black/white displays using the UC8151D controller."""

    source_files = ("epaper_spi_ultrachip.cpp", "epaper_spi_uc8151.cpp")

    def __init__(
        self, name: str, class_name: str = "EPaperUC8151", **defaults: Any
    ) -> None:
        defaults.setdefault("partial_update", True)
        super().__init__(name, class_name, **defaults)

    def get_init_sequence(self, config: dict) -> tuple:
        width, height = self.get_dimensions(config)
        return (
            # PANEL SETTING (black/white mode, LUT from OTP); the driver loads a register LUT for partial updates
            (0x00, 0x1F),
            # RESOLUTION SETTING: sources, then gates
            (0x61, width, height >> 8, height & 0xFF),
            # VCOM AND DATA INTERVAL SETTING: 1 in DTM2 is white
            (0x50, 0x97),
        )


# Good Display GDEW029T5, 2.9" mono 128x296 flexible panel. Replaces the waveshare_epaper model gdew029t5.
goodisplay_gdew029t5 = UC8151Mono(
    "goodisplay-gdew029t5-2.9",
    data_rate="10MHz",
    width_multiple=8,
    width=128,
    height=296,
)

# Seeed 2.9" flexible mono e-paper, SKU 104990852 (ribbon cable WFT0290CZ10)
goodisplay_gdew029t5.extend("seeed-104990852")

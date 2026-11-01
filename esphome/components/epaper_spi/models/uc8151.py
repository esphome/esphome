"""Black/white/red e-paper displays using the UltraChip UC8151D controller.

Supported models:
- uc8151: the controller on its own; dimensions and pins must be configured
- waveshare-2.13in-bv3: 104x212 pixels (2.13" B V3, EPD_2in13b_V3)

The busy pin of these displays is low while busy, so configure it with ``inverted: true``.
"""

import esphome.config_validation as cv
from esphome.const import CONF_DIMENSIONS
from esphome.types import ConfigType

from . import EpaperModel

MAX_SOURCES = 160
MAX_GATES = 296


class UC8151(EpaperModel):
    """EpaperModel class for black/white/red displays using the UC8151D controller."""

    class_name = "EPaperWaveshareBWR"
    source_files = ("epaper_waveshare_bwr.cpp",)

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

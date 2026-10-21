"""T133A01-based e-paper displays.

The T133A01 is a 6-color e-paper controller IC that drives large panels
(1200x1600 portrait). It uses a dual-CS SPI architecture where CS
controls one half of the pixel data and CS1 controls the other half,
as well as panel-level commands (power on, refresh, power off).

Supported models:
- Seeed-reTerminal-E1004: 1200x1600 pixels, 6-color (T133A01 panel)
"""

from typing import Any

from esphome.components.mipi import delay
from esphome.types import ConfigType

from .dualcs import BOTH, CMD66_DATA, PRIMARY, DualCSModel


class T133A01Model(DualCSModel):
    """DualCSModel subclass for T133A01-based 6-color e-paper displays."""

    source_files = DualCSModel.source_files + ("epaper_spi_t133a01.cpp",)

    def __init__(
        self, name: str, class_name: str = "EPaperT133A01", **defaults: Any
    ) -> None:
        super().__init__(name, class_name, **defaults)

    def get_init_sequence(self, config: ConfigType) -> tuple[tuple[int, ...], ...]:
        # The Arduino GFX library's EPD_INIT() (T133A01_Defines.h), with 10 ms after every command
        commands = (
            (0x74, PRIMARY, 0x00, 0x0C, 0x0C, 0xD9, 0xDD, 0xDD, 0x15, 0x15, 0x55),
            (0xF0, BOTH, *CMD66_DATA),
            (0x00, BOTH, 0xDF, 0x69),  # PSR
            (0xA5, PRIMARY, 0x44, 0x54, 0x00),  # DCDC
            (0x50, BOTH, 0x37),  # CDI
            (0x60, BOTH, 0x03, 0x03),  # TCON
            (0x86, BOTH, 0x10),  # AGID
            (0xE3, BOTH, 0x22),  # PWS
            self.resolution(config),  # TRES
            (0x01, PRIMARY, 0x0F, 0x00, 0x28, 0x2C, 0x28, 0x38),  # PWR
            (0xB6, PRIMARY, 0x07),  # EN_BUF
            (0x06, PRIMARY, 0xE0, 0x20),  # BTST_P
            (0xB7, PRIMARY, 0x01),  # BOOST_VDDP_EN
            (0x05, PRIMARY, 0xE0, 0x20),  # BTST_N
            (0xB0, PRIMARY, 0x01),  # BUCK_BOOST_VDDN
            (0xB1, PRIMARY, 0x02),  # TFT_VCOM_POWER
        )
        return tuple(entry for command in commands for entry in (command, delay(10)))


t133a01_base = T133A01Model(
    "t133a01",
    minimum_update_interval="30s",
    data_rate="10MHz",
)

# Seeed reTerminal E1004 - 13.3" 6-color e-paper (1200x1600, T133A01)
# Portrait orientation (1200 wide × 1600 tall), matching the Arduino
# Setup523 defines TFT_WIDTH=1200, TFT_HEIGHT=1600.
# CS and CS1 each receive half of each row's pixel data
# (300 bytes = 600 pixels per controller, for all 1600 rows).
Seeed_reTerminal_E1004 = t133a01_base.extend(
    "Seeed-reTerminal-E1004",
    width=1200,
    height=1600,
    cs_pin=10,
    cs1_pin=2,
    dc_pin=11,
    reset_pin=38,
    busy_pin={
        "number": 13,
        "inverted": True,
        "mode": {"input": True},
    },
    enable_pin=12,
)

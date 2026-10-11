"""Monochrome e-paper displays using the UltraChip UC8279 controller.

Supported models:
- uc8279: the controller on its own; dimensions and pins must be configured
- xteink-x4pro-uc8279: the Xteink X4 Pro e-reader (ESP32-S3) with its 800x480 UC8279 panel;
  the X4 Pro also ships with other panel controllers

The controller scans gate_offset + height gates, and the panel is bonded to the last
height of them; gate_offset is a property of the panel.
"""

from typing import Any

from esphome.const import CONF_DATA_RATE

from . import EpaperModel

GATE_OFFSET = "gate_offset"


class UC8279(EpaperModel):
    def __init__(
        self,
        name: str,
        class_name: str = "EPaperUC8279",
        data_rate: str = "10MHz",
        **defaults: Any,
    ) -> None:
        defaults.setdefault(CONF_DATA_RATE, data_rate)
        defaults.setdefault("partial_update", True)
        defaults.setdefault(GATE_OFFSET, 0)
        super().__init__(name, class_name=class_name, **defaults)

    def get_constructor_args(self, config) -> tuple:
        return (self.get_default(GATE_OFFSET),)

    # fmt: off
    def get_init_sequence(self, config: dict) -> tuple:
        width, height = self.get_dimensions(config)
        gates = self.get_default(GATE_OFFSET) + height
        return (
            (0x00, 0x37, 0x4D),  # panel setting; the driver switches it to the OTP LUT before each refresh
            (0x61, width >> 8, width & 0xFF, gates >> 8, gates & 0xFF),  # resolution: sources, gates
            (0x65, 0x00, 0x00, 0x00, 0x00),  # gate and source start
            (0x03, 0x20),  # power off sequence
            (0x30, 0x0E),  # PLL
            (0xE1, 0x02),  # gate scan
        )
    # fmt: on


uc8279 = UC8279("uc8279")

uc8279.extend(
    "xteink-x4pro-uc8279",
    width=800,
    height=480,
    gate_offset=120,
    cs_pin=13,
    dc_pin=18,
    reset_pin=14,
    busy_pin={"number": 6, "inverted": True},
    reset_duration="50ms",
)

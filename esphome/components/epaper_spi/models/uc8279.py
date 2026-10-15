"""Monochrome e-paper displays using the UltraChip UC8279 controller.

Supported models:
- uc8279: the controller on its own; dimensions and pins must be configured
- xteink-x4pro-uc8279: the Xteink X4 Pro e-reader (ESP32-S3) with its 800x480 UC8279 panel;
  the X4 Pro also ships with other panel controllers

The controller scans offset_height + height gates, and the panel is bonded to the last
height of them; offset_height (under dimensions) is a property of the panel.
"""

from typing import Any

import esphome.config_validation as cv
from esphome.const import CONF_DATA_RATE, CONF_DIMENSIONS, CONF_OFFSET_HEIGHT
from esphome.types import ConfigType

from . import EpaperModel, ultrachip_resolution


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
        defaults.setdefault(CONF_OFFSET_HEIGHT, 0)
        defaults.setdefault("width_multiple", 8)
        super().__init__(name, class_name=class_name, **defaults)

    def validate_config(self, config: ConfigType) -> ConfigType:
        _width, height = self.get_dimensions(config)
        if self.get_offset_height(config) + height > 0xFFFF:
            raise cv.Invalid(
                f"{CONF_OFFSET_HEIGHT} plus the panel height must not exceed {0xFFFF} gates",
                path=[CONF_DIMENSIONS, CONF_OFFSET_HEIGHT],
            )
        return config

    def get_constructor_args(self, config) -> tuple:
        return (self.get_offset_height(config),)

    # fmt: off
    def get_init_sequence(self, config: dict) -> tuple:
        width, height = self.get_dimensions(config)
        gates = self.get_offset_height(config) + height
        return (
            (0x00, 0x37, 0x4D),  # panel setting; the driver switches it to the OTP LUT before each refresh
            ultrachip_resolution(width, gates),  # resolution: sources, gates
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
    offset_height=120,
    cs_pin=13,
    dc_pin=18,
    reset_pin=14,
    busy_pin={"number": 6, "inverted": True},
    reset_duration="50ms",
)

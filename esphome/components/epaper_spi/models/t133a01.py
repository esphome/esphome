"""T133A01-based e-paper displays.

The T133A01 is a 6-color e-paper controller IC that drives large panels
(1200x1600 portrait). It uses a dual-CS SPI architecture where CS
controls one half of the pixel data and CS1 controls the other half,
as well as panel-level commands (power on, refresh, power off).

Supported models:
- Seeed-reTerminal-E1004: 1200x1600 pixels, 6-color (T133A01 panel)
"""

from typing import Any

from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_CS_PIN, CONF_DIMENSIONS
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import EpaperModel

CONF_CS1_PIN = "cs1_pin"
# Widest row the driver can stage: 4 * EPaperDualCS::MAX_HALF_ROW_BYTES (epaper_spi_dualcs.h),
# since each chip receives a quarter of the width in bytes per row.
MAX_WIDTH = 1600


class T133A01Model(EpaperModel):
    """EpaperModel subclass for T133A01-based 6-color e-paper displays."""

    source_files = (
        "epaper_spi_4bpp.cpp",
        "epaper_spi_dualcs.cpp",
        "epaper_spi_t133a01.cpp",
    )

    # The driver drives CS and CS1 directly for the dual-CS protocol.
    manages_cs = True

    def __init__(
        self, name: str, class_name: str = "EPaperT133A01", **defaults: Any
    ) -> None:
        super().__init__(name, class_name, **defaults)

    def get_constructor_args(self, config: ConfigType) -> tuple[bool]:
        # toggle_dc: this panel uses 4-wire SPI, so write_command_to_chip_() must toggle the
        # DC pin around the command byte.
        return (True,)

    def validate_config(self, config: ConfigType) -> ConfigType:
        width, height = self.get_dimensions(config)
        if width > MAX_WIDTH:
            raise cv.Invalid(
                f"{self.name} supports a width of at most {MAX_WIDTH}",
                path=[CONF_DIMENSIONS],
            )
        # Each chip gets half a row, packed 2 pixels per byte, and partial windows round
        # columns to groups of 4 and rows to pairs.
        if width % 4:
            raise cv.Invalid(
                f"{self.name} requires a width that is a multiple of 4",
                path=[CONF_DIMENSIONS],
            )
        if height % 2:
            raise cv.Invalid(
                f"{self.name} requires an even height", path=[CONF_DIMENSIONS]
            )
        return config

    def get_config_options(self) -> dict[Any, Any]:
        # CS1 is the second chip-select required by the dual-CS architecture.
        # fallback=None makes it required unless the model provides a default.
        return {
            self.option(CONF_CS1_PIN, fallback=None): pins.gpio_output_pin_schema,
        }

    async def to_code(self, var: MockObj, config: ConfigType) -> ConfigType:
        await cg.add_pin(var.set_primary_cs_pin, config[CONF_CS_PIN])
        await cg.add_pin(var.set_secondary_cs_pin, config[CONF_CS1_PIN])
        # Remove CS and CS1 from the config so that the base class doesn't try to handle them.
        return {k: v for k, v in config.items() if k not in (CONF_CS_PIN, CONF_CS1_PIN)}


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
